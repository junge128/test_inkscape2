#!/usr/bin/env bash
#
# SPDX-FileCopyrightText: 2022 René de Hesselle <dehesselle@web.de>
#
# SPDX-License-Identifier: GPL-2.0-or-later

#
# This script is CI-only, you will encounter errors if you run it on your
# local machine. If you want to build Inkscape locally, see
# https://gitlab.com/inkscape/deps/macos
#

# toolset release to build Inkscape
VERSION=1.2

# directory convenience handles
SELF_DIR=$(dirname "${BASH_SOURCE[0]}")
MIBAP_DIR=$SELF_DIR/mibap

git clone --single-branch https://gitlab.com/inkscape/deps/macos "$MIBAP_DIR"

if git -C "$MIBAP_DIR" checkout v"$VERSION"; then
  git -C "$MIBAP_DIR" submodule update --init --recursive

  # make sure the runner is clean (this doesn't hurt if there's nothing to do)
  "$MIBAP_DIR"/uninstall_toolset.sh

  if [ "$(basename -s .sh "${BASH_SOURCE[0]}")" = "test" ]; then
    # install build dependencies and Inkscape
    "$MIBAP_DIR"/install_toolset.sh restore_overlay
    # run the test suite
    if "$MIBAP_DIR"/310-inkscape_test.sh; then
      "$MIBAP_DIR"/uninstall_toolset.sh
    else
      # save testfiles only on failure
      "$MIBAP_DIR"/uninstall_toolset.sh save_testfiles
      exit 1
    fi
  else
    # install build dependencies
    "$MIBAP_DIR"/install_toolset.sh
    # build Inkscape
    if "$MIBAP_DIR"/build_inkscape.sh; then
      # uninstall build dependencies and archive build files
      "$MIBAP_DIR"/uninstall_toolset.sh save_overlay
    else
      "$MIBAP_DIR"/uninstall_toolset.sh
      exit 1
    fi
  fi
else
  echo "error: unknown version $VERSION"
  exit 1
fi
