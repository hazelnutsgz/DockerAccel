#!/bin/bash
cp draco_module.ko /tmp/
cd /tmp/
sudo insmod draco_module.ko
cd -
