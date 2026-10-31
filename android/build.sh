#!/bin/bash
# Builds SKIrf Wardriver for Android without Gradle:
#   aapt2 (resources) -> javac (Java 8 bytecode) -> dx (dex) -> zipalign -> apksigner
# Needs: tools/android.jar (API 34 with resources.arsc), tools/aapt2, dalvik-exchange, zipalign, apksigner, JDK,
# and the signing key: tools/release.jks, or set SKIRF_KEYSTORE / SKIRF_KS_PASS. The key is NOT in the public repo.
set -euo pipefail
cd "$(dirname "$0")"
T=tools
J=$T/android.jar
KS=${SKIRF_KEYSTORE:-$T/release.jks}
KP=${SKIRF_KS_PASS:?Set SKIRF_KS_PASS to the signing key password}
for f in $J $T/aapt2; do [ -f $f ] || { echo "Missing $f (see README.md, Building from source)"; exit 1; }; done
[ -f "$KS" ] || { echo "Missing signing key $KS. Use the project's key so updates install over the old app (see README.md)."; exit 1; }
O=build
rm -rf $O && mkdir -p $O/gen $O/cls
VER=$(sed -n 's/.*android:versionName="\([^"]*\)".*/\1/p' AndroidManifest.xml | tr ' ' '_')

$T/aapt2 compile --dir res -o $O/res.zip
$T/aapt2 link -I $J --manifest AndroidManifest.xml -A assets -o $O/base.apk --java $O/gen $O/res.zip \
    --min-sdk-version 26 --target-sdk-version 34 --auto-add-overlay

javac -nowarn -encoding UTF-8 -source 8 -target 8 -bootclasspath $J -Xlint:-options \
    -d $O/cls $(find src $O/gen -name '*.java') 2>&1 | grep -v "JAVA_TOOL_OPTIONS\|^Note:" || true
[ -f $O/cls/com/skirf/wardriver/MainActivity.class ] || { echo "javac failed"; exit 1; }

dalvik-exchange --dex --min-sdk-version=26 --output=$O/classes.dex $O/cls 2>&1 | grep -v JAVA_TOOL_OPTIONS || true
[ -f $O/classes.dex ] || { echo "dx failed"; exit 1; }

cp $O/base.apk $O/unsigned.apk
(cd $O && zip -q unsigned.apk classes.dex)
zipalign -f -p 4 $O/unsigned.apk $O/aligned.apk
apksigner sign --ks "$KS" --ks-pass pass:$KP --key-pass pass:$KP \
    --out $O/SKIrf_Wardriver_$VER.apk $O/aligned.apk 2>&1 | grep -v JAVA_TOOL_OPTIONS || true
apksigner verify $O/SKIrf_Wardriver_$VER.apk 2>&1 | grep -v JAVA_TOOL_OPTIONS || true
ls -la $O/SKIrf_Wardriver_$VER.apk
