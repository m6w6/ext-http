#!/bin/bash

set -e
exec 1> /dev/null

pushd $(dirname $(dirname $0))

COVERITY_BIN=$(PATH="$PATH:/opt/coverity/bin" which cov-build)
COVERITY_PROJECT=$(git ls-remote --get-url origin | sed 's/^.*:\(.*\)\.git$/\1/')
COVERITY_BUILD_DATE=$(date --iso-8601=seconds)
COVERITY_BUILD_BRANCH=$(git rev-parse --abbrev-ref HEAD)
COVERITY_BUILD_DIR=$(mktemp -td coverity.XXXXXX)

COVERITY_SCAN_TOKEN=$(gpg --quiet --decrypt < .coverity.asc)

phpize
./configure
make clean


$COVERITY_BIN --dir $COVERITY_BUILD_DIR/cov-int make -j8

echo >&2
echo -n "Submit results to scan.coverity.com? (y/N) " >&2
read submit
echo >&2

if test "$submit" != "y"; then
	exit
fi

pushd $COVERITY_BUILD_DIR
tar -czf cov-int{.tgz,}
popd

curl -sS\
	--form "token=$COVERITY_SCAN_TOKEN" \
	--form "version=$COVERITY_BUILD_BRANCH" \
	--form "email=mike@php.net" \
	--form "description=$COVERITY_BUILD_DATE" \
	--form "file=@$COVERITY_BUILD_DIR/cov-int.tgz" \
	--url "https://scan.coverity.com/builds?project=$COVERITY_PROJECT" >&2

rm -r $COVERITY_BUILD_DIR

popd
