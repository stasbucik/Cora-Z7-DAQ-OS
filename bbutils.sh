unset LD_LIBRARY_PATH
source components/yocto/environment-setup-cortexa9t2hf-neon-xilinx-linux-gnueabi
source components/yocto/layers/poky/oe-init-build-env
export PROOT="$(pwd)"
export BB_ENV_PASSTHROUGH_ADDITIONS="$BB_ENV_PASSTHROUGH_ADDITIONS PETALINUX PROOT"
