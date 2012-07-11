cd $TEMP_DIR
$ORI_EXE clone localhost:$SOURCE_REPO $TEST_REPO2
cd $TEST_REPO2
$ORI_EXE checkout
$PYTHON $SCRIPTS/compare.py "$SOURCE_REPO" "$TEST_REPO2"
$ORI_EXE verify
$ORI_EXE stats

cd $TEMP_DIR
rm -rf $TEST_REPO2
