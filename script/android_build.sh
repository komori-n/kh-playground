#!/bin/bash

JOBS=`grep -c ^processor /proc/cpuinfo 2>/dev/null`

EDITIONS='*'
EXTRA=''

while getopts e:x: OPT
do
  case $OPT in
    e) EDITIONS="$OPTARG"
      ;;
    x) EXTRA="$OPTARG"
      ;;
  esac
done

set -f
IFS=, eval 'EDITIONSARR=($EDITIONS)'

pushd `dirname $0`
pushd ..

EDITIONS=(
  YANEURAOU_ENGINE_NNUE
  YANEURAOU_ENGINE_NNUE_HALFKP_VM_256X2_32_32
  YANEURAOU_ENGINE_NNUE_HALFKPE9
  YANEURAOU_ENGINE_NNUE_KP256
  YANEURAOU_ENGINE_KPPT
  YANEURAOU_ENGINE_KPP_KKPT
  YANEURAOU_ENGINE_MATERIAL
  YANEURAOU_ENGINE_MATERIAL2
  YANEURAOU_ENGINE_MATERIAL3
  YANEURAOU_ENGINE_MATERIAL4
  YANEURAOU_ENGINE_MATERIAL5
  YANEURAOU_ENGINE_MATERIAL6
  YANEURAOU_ENGINE_MATERIAL7
  YANEURAOU_ENGINE_MATERIAL8
  YANEURAOU_ENGINE_MATERIAL9
#  YANEURAOU_ENGINE_MATERIAL10
  YANEURAOU_MATE_ENGINE
  TANUKI_MATE_ENGINE
  USER_ENGINE
)

declare -A EDITIONSTR;
EDITIONSTR=(
  ["YANEURAOU_ENGINE_NNUE"]="YANEURAOU_ENGINE_NNUE"
  ["YANEURAOU_ENGINE_NNUE_HALFKP_VM_256X2_32_32"]="YANEURAOU_ENGINE_NNUE_HALFKP_VM_256X2_32_32"
  ["YANEURAOU_ENGINE_NNUE_HALFKPE9"]="YANEURAOU_ENGINE_NNUE_HALFKPE9"
  ["YANEURAOU_ENGINE_NNUE_KP256"]="YANEURAOU_ENGINE_NNUE_KP256"
  ["YANEURAOU_ENGINE_KPPT"]="YANEURAOU_ENGINE_KPPT"
  ["YANEURAOU_ENGINE_KPP_KKPT"]="YANEURAOU_ENGINE_KPP_KKPT"
  ["YANEURAOU_ENGINE_MATERIAL"]="YANEURAOU_ENGINE_MATERIAL"
  ["YANEURAOU_ENGINE_MATERIAL2"]="YANEURAOU_ENGINE_MATERIAL MATERIAL_LEVEL=2"
  ["YANEURAOU_ENGINE_MATERIAL3"]="YANEURAOU_ENGINE_MATERIAL MATERIAL_LEVEL=3"
  ["YANEURAOU_ENGINE_MATERIAL4"]="YANEURAOU_ENGINE_MATERIAL MATERIAL_LEVEL=4"
  ["YANEURAOU_ENGINE_MATERIAL5"]="YANEURAOU_ENGINE_MATERIAL MATERIAL_LEVEL=5"
  ["YANEURAOU_ENGINE_MATERIAL6"]="YANEURAOU_ENGINE_MATERIAL MATERIAL_LEVEL=6"
  ["YANEURAOU_ENGINE_MATERIAL7"]="YANEURAOU_ENGINE_MATERIAL MATERIAL_LEVEL=7"
  ["YANEURAOU_ENGINE_MATERIAL8"]="YANEURAOU_ENGINE_MATERIAL MATERIAL_LEVEL=8"
  ["YANEURAOU_ENGINE_MATERIAL9"]="YANEURAOU_ENGINE_MATERIAL MATERIAL_LEVEL=9"
  ["YANEURAOU_ENGINE_MATERIAL10"]="YANEURAOU_ENGINE_MATERIAL MATERIAL_LEVEL=10"
  ["YANEURAOU_MATE_ENGINE"]="YANEURAOU_MATE_ENGINE"
  ["TANUKI_MATE_ENGINE"]="TANUKI_MATE_ENGINE"
  ["USER_ENGINE"]="USER_ENGINE"
);

declare -A DIRSTR;
DIRSTR=(
  ["YANEURAOU_ENGINE_NNUE"]="NNUE"
  ["YANEURAOU_ENGINE_NNUE_HALFKP_VM_256X2_32_32"]="NNUE_HALFKP_VM"
  ["YANEURAOU_ENGINE_NNUE_HALFKPE9"]="NNUE_HALFKPE9"
  ["YANEURAOU_ENGINE_NNUE_KP256"]="NNUE_KP256"
  ["YANEURAOU_ENGINE_KPPT"]="KPPT"
  ["YANEURAOU_ENGINE_KPP_KKPT"]="KPP_KKPT"
  ["YANEURAOU_ENGINE_MATERIAL"]="MaterialLv1"
  ["YANEURAOU_ENGINE_MATERIAL2"]="MaterialLv2"
  ["YANEURAOU_ENGINE_MATERIAL3"]="MaterialLv3"
  ["YANEURAOU_ENGINE_MATERIAL4"]="MaterialLv4"
  ["YANEURAOU_ENGINE_MATERIAL5"]="MaterialLv5"
  ["YANEURAOU_ENGINE_MATERIAL6"]="MaterialLv6"
  ["YANEURAOU_ENGINE_MATERIAL7"]="MaterialLv7"
  ["YANEURAOU_ENGINE_MATERIAL8"]="MaterialLv8"
  ["YANEURAOU_ENGINE_MATERIAL9"]="MaterialLv9"
  ["YANEURAOU_ENGINE_MATERIAL10"]="MaterialLv10"
  ["YANEURAOU_MATE_ENGINE"]="YaneuraOu_MATE"
  ["TANUKI_MATE_ENGINE"]="tanuki_MATE"
  ["USER_ENGINE"]="KomoringHeights"
);

set -f

for EDITION in ${EDITIONS[@]}; do
  for EDITIONPTN in ${EDITIONSARR[@]}; do
    set +f
    if [[ $EDITION == $EDITIONPTN ]]; then
      set -f
      echo "* edition: ${EDITION}"
      BUILDDIR=build/android/${DIRSTR[$EDITION]}
      mkdir -p ${BUILDDIR}
      ndk-build clean YANEURAOU_EDITION=${EDITIONSTR[$EDITION]} ${EXTRA}
      ndk-build -j${JOBS} YANEURAOU_EDITION=${EDITIONSTR[$EDITION]} V=1 ${EXTRA} >& >(tee ${BUILDDIR}/build.log) || exit $?
      bash -c "cp libs/**/* ${BUILDDIR}"
      ndk-build clean YANEURAOU_EDITION=${EDITIONSTR[$EDITION]} ${EXTRA}
      break
    fi
    set -f
  done
done

popd
popd
