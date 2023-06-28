#!/bin/bash

if [ "$#" -eq 0 ]; then
    dirs=$(ls -d tests*/ | tr '\n' ',' | sed 's/,$//')
    echo "Directories to Test: ${dirs}"
else
    dirs="$@"
    echo "Directories to Test: ${dirs}"
fi

IFS=',' read -ra test_dirs <<< "$dirs"

declare -A dir_statistics

passed_dirs=()
passed_tests=()
failed_tests=()
total_passed_tests_count=0
total_tests_count=0

echo
echo "Running Tests..."
echo "----------------"

for dir in "${test_dirs[@]}"; do
  dir=${dir%/}  # Remove trailing slash from directory name
  if [ ! -d "$dir" ]; then
    echo "Directory '$dir' does not exist."
    continue
  fi

  success=1
  passed_tests_count=0
  tests_count=0

  for file in $(ls $dir/ | grep in | cut -d"." -f1); do
    ./hw5 < $dir/${file}.in > $dir/${file}.lli
    h=$(head $dir/${file}.lli)
    if [[ $h = line* ]] || [[ $h = Program* ]]; then
      cp $dir/${file}.lli $dir/${file}.res
    else
      lli < $dir/${file}.lli > $dir/${file}.res
    fi
    diff $dir/$file.res $dir/${file}.out
    retval=$?
    if [ $retval -ne 0 ]; then
        echo -e "\e[31mFailed test $file\e[0m"
        success=0
        failed_tests+=("$file")
    else
        echo -e "\e[32mPassed test $file\e[0m"
        passed_tests+=("$file")
        passed_tests_count=$((passed_tests_count + 1))
    fi
    tests_count=$((tests_count + 1))
  done

  if [ $success -eq 1 ]; then
      echo -e "\e[32mPassed all ${dir}\e[0m"
      passed_dirs+=("${dir}")
  else
      echo -e "\e[31mFailed some of the ${dir}\e[0m"
  fi

  dir_statistics["$dir"]="$passed_tests_count/$tests_count"
  total_passed_tests_count=$((total_passed_tests_count + passed_tests_count))
  total_tests_count=$((total_tests_count + tests_count))
done

echo
echo "Summary:"
echo "--------"
echo -e "Passed Dirs: \e[32m${passed_dirs[*]}\e[0m"
echo -e "Passed Tests: \e[32m${passed_tests[*]}\e[0m"
echo -e "Failed Tests: \e[31m${failed_tests[*]}\e[0m"
echo "Total Passed Tests: ${total_passed_tests_count}"
echo "Total Tests: ${total_tests_count}"
total_percentage=$(awk "BEGIN { printf \"%.2f\", ${total_passed_tests_count} * 100 / ${total_tests_count} }")
echo "Total Percentage of Passed Tests: ${total_percentage}%"

echo
echo "Statistics for each directory:"
echo "-----------------------------"
for dir in "${!dir_statistics[@]}"; do
  passed_tests_count="${dir_statistics["$dir"]%/*}"
  tests_count="${dir_statistics["$dir"]#*/}"
  percentage=$(awk "BEGIN { printf \"%.2f\", $passed_tests_count * 100 / $tests_count }")
  echo "Directory: $dir"
  echo "  Passed Tests: ${dir_statistics["$dir"]}"
  echo "  Percentage of Passed Tests: ${percentage}%"
  echo
done
