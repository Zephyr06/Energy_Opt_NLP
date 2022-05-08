#!/usr/bin/bash

# ************** Adjust settings there **************
title="DagPerformance"
MaxTaskNumber=5
ROOT_PATH="/home/zephyr/Programming/Energy_Opt_NLP"
# ***************************************************

cp parameters.yaml $ROOT_PATH/sources/parameters.yaml
# clear buffer file content
cd $ROOT_PATH/CompareWithBaseline
python clear_result_files.py  --folder $title

python edit_yaml.py --entry "batchOptimizeFolder" --value $title
python edit_yaml.py --entry "core_m_dag" --value 16

perform_optimization() {
	# Optimize energy consumption
	cd $ROOT_PATH/release
	make -j8
	./tests/DAGBatch
	cd $ROOT_PATH/CompareWithBaseline
	sleep 1
}


for (( jobNumber=3; jobNumber<=$MaxTaskNumber; jobNumber++ ))
do
	# generate task set
	
	echo "$title iteration is: $jobNumber"
	
	# dog-leg, eliminated, approximated Jacobian
    	python $ROOT_PATH/CompareWithBaseline/edit_yaml.py --entry "optimizerType" --value 1
	python $ROOT_PATH/CompareWithBaseline/edit_yaml.py --entry "exactJacobian" --value 0
	python $ROOT_PATH/CompareWithBaseline/edit_yaml.py --entry "elimIte" --value 1000
	perform_optimization
	
	# dog-leg, not eliminated, exact Jacobian
	python $ROOT_PATH/CompareWithBaseline/edit_yaml.py --entry "optimizerType" --value 1
	python $ROOT_PATH/CompareWithBaseline/edit_yaml.py --entry "exactJacobian" --value 0
	python $ROOT_PATH/CompareWithBaseline/edit_yaml.py --entry "elimIte" --value 0
	perform_optimization
	
done



# visualize the result
cd $ROOT_PATH/CompareWithBaseline/$title
python $ROOT_PATH/CompareWithBaseline/$title/Visualize_performance.py  --minTaskNumber 5 --title $title  --maxTaskNumber $MaxTaskNumber --data_source "EnergySaveRatio"
python $ROOT_PATH/CompareWithBaseline/$title/Visualize_performance.py  --minTaskNumber 5 --title $title  --maxTaskNumber $MaxTaskNumber --data_source "Time"