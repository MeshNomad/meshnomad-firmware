#!/bin/sh
# Writes .cliffignore: every commit that is not a MeshNomad commit (MeshCore history and merged
# upstream releases), so git-cliff lists only MeshNomad changes. MeshNomad commits are those by
# the MeshNomad account or Renovate. Run from the repository root before git-cliff.
set -eu
git rev-list --all --perl-regexp --author='^(?!MeshNomad |renovate\[bot\] )' > .cliffignore
echo "cliff_ignore: $(wc -l < .cliffignore) commits ignored"
