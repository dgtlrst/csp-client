import subprocess
import pathlib
import re
import sys
import glob
import os
import argparse
import cformat_exceptions

CFG_VERBOSE = False


def trace(msg: str) -> None:
    if CFG_VERBOSE:
        print(msg)


def is_entry_included(entry_name: str) -> bool:
    # ensure that all regexp can use only forward slash '/' to specify folders in the entry
    # regardless of the platform used, hence comparisons will always work properly
    normalized_path_entry = str(entry_name).replace(os.sep, "/")

    for exc in cformat_exceptions.exceptions_list:
        match = re.search(exc, normalized_path_entry)

        if match != None:
            return False

    return True


def check_file_format(file_path: pathlib.Path) -> bool:
    if is_entry_included(file_path.resolve()):
        if (file_path.name.endswith('.c')) or (file_path.name.endswith('.h')):
            trace(f'checking format of `{file_path.resolve()}`...')

            formatter_output = subprocess.run(
                ['clang-format', '--dry-run', file_path.resolve()], capture_output=True).stderr

            decoded_output = formatter_output.decode(encoding="utf-8")

            if decoded_output != '':
                if ('warning:' in decoded_output) or ('error:' in decoded_output):
                    return False
    else:
        trace(f'skipping `{file_path}`...')

    return True


def run_formatter(file_path: pathlib.Path, dry_run: bool) -> None:
    if is_entry_included(file_path.resolve()):
        trace(f'formatting file `{file_path.resolve()}`...')

        format_cmd = ['clang-format', '-i',
                      str(file_path.resolve())]

        if dry_run:
            format_cmd.append('--dry-run')

        subprocess.run(format_cmd)
    else:
        trace(f'skipping `{file_path}`...')


def format_file_list(file_list: list[str]) -> None:
    print('formatting files...')
    for file_entry in file_list:
        file_path = pathlib.Path(file_entry).resolve()

        run_formatter(file_path, dry_run=False)


def format_source_code(root_path: pathlib.Path) -> None:
    file_list = list(root_path.rglob('*.c'))
    file_list.extend(list(root_path.rglob('*.h')))

    format_file_list(file_list)


def verify_changed_source_code_format(format_violations: bool = False, report_progress: bool = False) -> None:
    format_violations_list: list[str] = []
    working_indicators: str = '/-\\|'
    git_cmd = ['git', 'diff', '--cached', '--name-only']                                  

    git_get_toplevel_folder_cmd = ['git', 'rev-parse', '--show-toplevel']

    toplevel_folder = pathlib.Path(subprocess.run(git_get_toplevel_folder_cmd,
                                  capture_output=True, text=True).stdout.splitlines(keepends=False)[0])

    print(f'checking modified files reported by git...')
    changes_list = subprocess.run(git_cmd,
                                  capture_output=True, text=True).stdout.splitlines(keepends=False)
    file_cntr = 1
    pattern = re.compile('(.*?)$')
    for mod_entry in changes_list:
        if report_progress:
            percentage: float = file_cntr * 100.0 / len(changes_list)
            print(
                f' {working_indicators[file_cntr % len(working_indicators)]} {file_cntr} / {len(changes_list)} => {percentage:.2f}%', end='\r')

        file_cntr += 1

        match = pattern.search(mod_entry)
        if match != None:
            file_path = toplevel_folder / pathlib.Path(match.group(1))
            if is_entry_included(file_path.resolve()):
                if not check_file_format(file_path):
                    trace(
                        f'format verification failed for file: `{file_path}`!')
                    format_violations_list.append(str(file_path.resolve()))

    if format_violations:
        print('formatting source code formatting violations...')
        format_file_list(format_violations_list)
    else:
        if (len(format_violations_list) > 0):
            title = 'Summary of files which violate formatting rules:'
            print('-' * len(title))
            print(title)
            print('-' * len(title))

            for violation in format_violations_list:
                print(f'{violation}')

            print("Use `py cformat.py -c -fv` to fix all reported violations automatically or fix each of the entries manually one-by-one.")
            sys.exit(1)
        else:
            print('no formatting violations found on current repo')

    sys.exit(0)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description='This script performs C source files format validation and corrections using clang-format. It will return an exit code 1 if there are files not matching the used formatting rules, otherwise it will return exit code 0.')

    parser.add_argument('-fv', '--force-format-violations', action='store_true', default=False,
                        help='Performs formatting of all files which do not comply with the current formatting rules')

    parser.add_argument('-v', '--verbose', action='store_true', default=False,
                        help='Activate verbose printing of the progress')

    parser.add_argument('-p', '--progress', action='store_true', default=False,
                        help='Activate progress reporting on the command line')

    parser.add_argument('-c', '--check', action='store_true', default=False,
                        help='If specified, the script verifies the formatting, otherwise it performs in-place formatting of files in the source location given')

    parser.add_argument('-src-path', '--source-path', type=pathlib.Path,
                        default=None, help='formats all source files in the specified location recursively')

    if len(sys.argv) == 1:
        parser.print_help(sys.stderr)
        sys.exit(1)

    try:
        args = parser.parse_args()

        CFG_VERBOSE = args.verbose

        if args.check:
            verify_changed_source_code_format(
                args.force_format_violations, args.progress)
        else:
            if args.source_path != None:
                if args.source_path.is_file():
                    run_formatter(args.source_path, dry_run=False)
                else:
                    format_source_code(args.source_path)
            else:
                print('\nPlease specify `--source-path` to use for formatting or the `--check` option to verify modified files formatting.\n')

                parser.print_help(sys.stderr)
                sys.exit(1)
    except Exception as e:
        print(
            f'Ooops... This is kind of embarassing. We have an exception:\n\n>>> {e} <<<')
        sys.exit(1)
