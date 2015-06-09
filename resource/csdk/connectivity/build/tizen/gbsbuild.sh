#!/bin/sh

cur_dir="./resource/csdk/connectivity/"

spec=`ls ./resource/csdk/connectivity/build/tizen/packaging/*.spec`
version=`rpm --query --queryformat '%{version}\n' --specfile $spec`

name=`echo $name|cut -d" " -f 1`
version=`echo $version|cut -d" " -f 1`

name=oicca

echo $1
export TARGET_TRANSPORT=$1

echo $2
export SECURED=$2

echo $3
export BUILD_SAMPLE=$3

echo $4
export RELEASE=$4

echo $5
export LOGGING=$5


echo $TARGET_TRANSPORT
echo $BUILD_SAMPLE

rm -rf $name-$version

builddir=`pwd`
sourcedir=`pwd`

echo `pwd`

mkdir ./tmp
mkdir ./tmp/con/
cp -R $cur_dir/* $sourcedir/tmp/con
cp -R $cur_dir/SConscript $sourcedir/tmp/con
cp -R $cur_dir/src/ip_adapter/SConscript $sourcedir/tmp/con/src/ip_adapter/
cp -R $cur_dir/src/bt_le_adapter/SConscript $sourcedir/tmp/con/src/bt_le_adapter/
cp -R $cur_dir/src/bt_edr_adapter/SConscript $sourcedir/tmp/con/src/bt_edr_adapter/
cp -R $cur_dir/common/SConscript $sourcedir/tmp/con/common/
cp -R $cur_dir/lib/libcoap-4.1.1/SConscript $sourcedir/tmp/con/lib/libcoap-4.1.1/
cp -R $cur_dir/samples/tizen/ $sourcedir/tmp/con/sample/
mkdir -p $sourcedir/tmp/con/sample/lib/tizen/ble/libs
cp -R $cur_dir/lib/tizen/ble/libs/* $sourcedir/tmp/con/sample/lib/tizen/ble/libs/

cd $sourcedir
cd $cur_dir/build/tizen

cp -R ./* $sourcedir/tmp/
rm -f $sourcedir/tmp/SConscript
cp SConstruct $sourcedir/tmp/
cp scons/SConscript $sourcedir/tmp/scons/

mkdir -p $sourcedir/tmp/iotivityconfig
cd $sourcedir/build_common/
cp -R ./iotivityconfig/* $sourcedir/tmp/iotivityconfig/
cp -R ./SConscript $sourcedir/tmp/

cd $sourcedir/tmp

echo `pwd`

whoami
# Initialize Git repository
if [ ! -d .git ]; then
   git init ./
   git config user.email "you@example.com"
   git config user.name "Your Name"
   git add ./
   git commit -m "Initial commit"
fi

echo "Calling core gbs build command"
gbscommand="gbs build -A armv7l --include-all --define 'TARGET_TRANSPORT $1' --define 'SECURED $2' --define 'RELEASE $4' --define 'LOGGING $5' --repository ./"
echo $gbscommand
if eval $gbscommand; then
   echo "Core build is successful"
else
   echo "Core build failed. Try 'sudo find . -type f -exec dos2unix {} \;' in the 'connectivity/' folder"
   cd $sourcedir
   rm -rf $sourcedir/tmp
   exit
fi

if echo $BUILD_SAMPLE|grep -qi '^ON$'; then
   cd con/sample
   echo `pwd`
   # Initialize Git repository
   if [ ! -d .git ]; then
      git init ./
      git config user.email "you@example.com"
      git config user.name "Your Name"
      git add ./
      git commit -m "Initial commit"
   fi
   echo "Calling sample gbs build command"
   gbscommand="gbs build -A armv7l --include-all --define 'TARGET_TRANSPORT $1' --define 'SECURED $2' --define 'LOGGING $5' --repository ./"
   echo $gbscommand
   if eval $gbscommand; then
      echo "Sample build is successful"
   else
      echo "Sample build is failed. Try 'sudo find . -type f -exec dos2unix {} \;' in the 'connectivity/' folder"
   fi
else
	echo "Sample build is not enabled"
fi


cd $sourcedir
rm -rf $sourcedir/tmp

