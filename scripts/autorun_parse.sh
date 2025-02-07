#!/usr/bin/env bash
# Requirement: A file named autorun_process_file in this script's directory
# that contains newline-separated process names, formatted like like so:
# <app> <# of gateway> <object size pages> <workload> <# of process>
# Example:
#   zstore small wrk_read 10
#   ceph medium wrk_write 10
#   minio large s3bench 10
source $(dirname $0)/utils.sh

# Get the config parameter specified in $1 for app = $autorun_app
function get_from_config() {
  config_file=$autorun_home/scripts/config/$autorun_app
  if [ ! -f "$config_file" ]; then
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
autorun_out_prefix="/tmp/${autorun_app}_out"
autorun_err_prefix="/tmp/${autorun_app}_err"

autorun_object_size_pages=`get_from_config "object_size_pages"`
autorun_num_processes=`get_from_config "num_processes"`
autorun_num_gateways=`get_from_config "num_gateways"`
autorun_ip_addr=`get_from_config "ip_addr"`

INPUT=`get_from_config "app"`
autorun_app_name=$(echo $INPUT| cut -d'_' -f 2)

INPUT=`get_from_config "workload"`
autorun_gen=$(echo $INPUT| cut -d'_' -f 1)
autorun_workload=$(echo $INPUT| cut -d'_' -f 2)

blue "autorun: app = $autorun_app_name, num gateways = $autorun_num_gateways, generator = $autorun_gen, workload = $autorun_workload, num_processes = $autorun_num_processes"
