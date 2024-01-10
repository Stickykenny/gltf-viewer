cmake -S . -B build -DCMAKE_INSTALL_PREFIX=./dist -DCMAKE_BUILD_TYPE=Release
cmake --build build -j --target install --config release
ROOT=$(pwd)
OUTDIR=`date +%Y-%m-%d-%H-%M-%S`
mkdir -p $ROOT/output-images/$OUTDIR
pushd gltf-sample-models/2.0/
for f in */; do
  f=$(echo "$f" | sed 's:/*$::')
  $ROOT/dist/gltf-viewer viewer "$f/glTF/$f.gltf" --output "$ROOT/output-images/$OUTDIR/$f.png"
done
popd
