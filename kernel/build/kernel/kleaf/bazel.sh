#!/bin/bash -e
#oplus add a wrapper to enable remote cache
#move original bazel.sh into bazel.origin.sh
my_dir=$(dirname $(readlink -f "$0"))
original_sh=$my_dir/bazel.origin.sh
if [[ "$OPLUS_USE_JFROG_CACHE" == "true" || "$OPLUS_USE_BUILDBUDDY_REMOTE_BUILD" == "true" ]];then
   if ! $original_sh "$@" ; then
      echo "remote cache build fail! retry local build"
      OPLUS_USE_BUILDBUDDY_REMOTE_BUILD="false" OPLUS_USE_JFROG_CACHE="false" $original_sh "$@"
   fi
else
   $original_sh "$@"
fi
