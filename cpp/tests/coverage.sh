make clean

make

lcov --initial \
  --directory . \
  --directory ../lib/include \
  --directory ../lib/src \
  --base-directory . \
  --gcov-tool ./llvm-gcov.sh \
  --capture \
  --no-external \
  --output coverage_base.info

for TEST in ./*Test
do
    ./$TEST
done

lcov --directory . \
  --directory ../lib/include \
  --directory ../lib/src \
  --base-directory . \
  --gcov-tool ./llvm-gcov.sh \
  --capture \
  --no-external \
  --output coverage_test.info

# Collect the code coverage results
lcov -a coverage_base.info \
  -a coverage_test.info \
  -output coverage.info

# Generate HTML files.
genhtml coverage.info -o output
