#!/usr/bin/env bash
# Requirement: A file named autorun_process_file in this script's directory
# that contains newline-separated process names, formatted like like so:
# <app> <# of gateway> <object size pages> <workload> <# of process>
# Example:
#   zstore 1 wrk_read 10
#   ceph 1 wrk_read 10
#   ceph 1 wrk_read 10
source $(dirname $0)/utils.sh

# Get the config parameter specified in $1 for app = $autorun_app
function get_from_config() {
  config_file=$autorun_home/scripts/config/$autorun_app
  if [ ! -f $config_file ]; then
    blue "autorun: App config file $config_file not found. Existing."
    exit
  fi

  config_param=`cat $config_file | grep $1 | cut -d ' ' -f 2`
  if [ -z $config_param ]; then
    blue "autorun: Parameter $1 absent in config file $config_file. Exiting."
    exit
  fi

  echo $config_param
}

# Variables set by the human user
autorun_home="$HOME/dev/zstore"

# Check autorun_app
assert_file_exists $autorun_home/scripts/autorun_app_file
export autorun_app=`cat $autorun_home/scripts/autorun_app_file`
# assert_file_exists $autorun_home/build/$autorun_app

# Variables exported by this script
# autorun_out_prefix="/tmp/${autorun_app}_out"
# autorun_err_prefix="/tmp/${autorun_app}_err"

autorun_object_size_pages=`get_from_config "object_size_pages"`

autorun_num_processes=`get_from_config "num_processes"`
autorun_num_gateways=`get_from_config "num_gateways"`

INPUT=`get_from_config "app"`
autorun_app_name=$(echo $INPUT| cut -d'_' -f 2)

INPUT=`get_from_config "workload"`
autorun_gen=$(echo $INPUT| cut -d'_' -f 1)
autorun_workload=$(echo $INPUT| cut -d'_' -f 2)

# autorun_process_file=$(dirname $0)/autorun_process_file
# if [ ! -f $autorun_process_file ]; then
#   blue "autorun: Server file not found. Exiting."
#   exit
# fi

# Check if the process file has sufficient processs
# autorun_process_file_num_processes=`cat $autorun_process_file | wc -l`
# if [ "$autorun_process_file_num_processes" -lt  "$autorun_num_processes" ]; then
#   blue "autorun: Too few processs in process file. Exiting."
#   exit
# fi

# Create autorun process arrays using the first $autorun_num_processes process
# names in autorun_process_file.
# process_idx="0"
# while read dns_name udp_port numa; do
#   if [[ -z $dns_name || -z $udp_port || -z $numa ]]; then
#     blue "Invalid autorun file. name = $dns_name, udp = $udp_port, numa = $numa"
#     blue "Exiting"
#     exit
#   fi
#
#   autorun_name_list[$process_idx]=$dns_name
#   autorun_udp_port_list[$process_idx]=$udp_port
#   autorun_numa_list[$process_idx]=$numa
#   ((process_idx+=1))
# done < $autorun_process_file

blue "autorun: app = $autorun_app_name, num gateways = $autorun_num_gateways, generator = $autorun_gen, workload = $autorun_workload, num_processes = $autorun_num_processes"
