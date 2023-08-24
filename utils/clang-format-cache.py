#!/usr/bin/env python3

'''clang format the files in the index (aka the staged files).

@see https://github.com/gerbv/gerbv/pull/203
@author eyal0
'''

import os
import re
import subprocess
import sys
import fnmatch
import difflib
import argparse


def find_executable(filename_regex):
  '''Find executables by regex and yield them all.'''
  for path in os.environ["PATH"].split(":"):
    if not os.path.isdir(path):
      continue
    for filename in os.listdir(path):
      if not re.match(filename_regex, filename):
        continue
      full_path = os.path.join(path, filename)
      if os.access(full_path, os.X_OK):
        yield full_path


def find_clang_format(min_major_version):
  '''Find the first clang-format with a minimum major version.'''
  clang_formatters = find_executable("clang-format(-\d+)?$")
  for clang_formatter in clang_formatters:
    version = subprocess.check_output([clang_formatter, "--version"])
    major_version = re.search("\d+", str(version))
    if int(major_version.group()) >= min_major_version:
      return clang_formatter


def get_changed_files():
  '''Find all changed files in the index.

  Yield each file in the format (octal_mode, filename).

  '''
  git_status_output = subprocess.check_output(["git", "status", "--porcelain=v2"])
  for line in git_status_output.splitlines():
    status_split = line.decode().split()
    #print(status_split)
    if status_split[0] not in ("1", "2"): # Neither modified, new, nor renamed file.
      continue
    if status_split[1][0] not in "MARC":
      # Ignore unmerged and deleted files.
      continue
    index_mode = status_split[4] # This is the octal file mode like 100644.
    # The math below finds the correct target filename.
    target_file_name = status_split[7 + int(status_split[0])]
    yield (index_mode, target_file_name)


def parse_arguments():
  parser = argparse.ArgumentParser(description='clang-format all files in the index.')
  parser.add_argument('-n', '--dry_run', action="store_true",
                      help="Don't update the files, just print the diffs.")
  return parser.parse_args()


def get_formatted_file(clang_format_executable, file_contents):
  return subprocess.run([clang_format_executable, "-Werror", "--style=file"],
                        check=True,
                        input=file_contents,
                        capture_output=True).stdout


def get_git_blob(file_contents):
  return subprocess.run(['git', 'hash-object', '-w', '--stdin'],
                        check=True,
                        input=file_contents,
                        capture_output=True).stdout.decode().strip()


def update_index(mode, blob, target_file):
  subprocess.check_output(['git', 'update-index', '--add',
                           '--cacheinfo', f'{mode},{blob},{target_file}'])


def main():
  args = parse_arguments()
  clang_format_executable = find_clang_format(min_major_version=14)
  if not clang_format_executable:
    sys.stderr.write(sys.argv[0] + ": clang-format executable not found\n")
    sys.exit(1)

  changed_files = get_changed_files()
  for mode, target_file in changed_files:
    if not any(fnmatch.fnmatch(target_file, x) for x in ["src/*.h", "src/*.c", "src/*.cpp"]):
      # This is not a c/c++ file.  Keep this in sync with the list in
      # .github/workflows/ci.yaml.
      continue

    file_contents = subprocess.check_output(["git", "show", ":" + target_file])
    formatted_file_contents = get_formatted_file(clang_format_executable, file_contents)
    if formatted_file_contents == file_contents:
      continue
    diff = difflib.unified_diff(
      file_contents.decode().splitlines(True),
      formatted_file_contents.decode().splitlines(True),
      fromfile=os.path.join("old", target_file),
      tofile=os.path.join("new", target_file))
    if args.dry_run:
      # Just print to the screen without making the change.
      sys.stdout.writelines(diff)
    else:
      new_blob = get_git_blob(formatted_file_contents)
      update_index(mode, new_blob, target_file)
      # Let's also apply these changes into the worktree because the
      # user probably will expect that.
      difftext = ''.join(diff)
      patch_process = subprocess.run(['patch', '-p' ,'1'],
                                     check=True,
                                     encoding='utf8',
                                     input=difftext,
                                     capture_output=True)
  if args.dry_run:
    print(f"\n\nTo apply this patch, run: {sys.argv[0]} -n | patch -p 1")


if __name__ == "__main__":
  main()
