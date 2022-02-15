.PHONY: all
all: workflows/ci.yml
all: workflows/curl-matrix.yml

workflows/%.yml: ../scripts/gen_github_workflow_%.php
	$<>$@

workflows/curl-matrix.yml: ../scripts/curlver.dist
