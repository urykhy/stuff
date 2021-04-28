#!/bin/bash

set -e
ROOT="https://raw.githubusercontent.com/swagger-api/swagger-ui/master/dist"

mkdir tmp_ui
pushd tmp_ui
wget $ROOT/{swagger-ui.css,swagger-ui-bundle.js,swagger-ui-standalone-preset.js}
cp ../../*.yaml .
cp ../../index.html .
tar -cvf ../swagger_ui.tar *
popd
rm -rf ./tmp_ui
