
BOOT_IMG=/wwiv/freedos.img
DOORS=/wwiv/doors
INST=/wwiv/e/1/temp
export TERM=linux 
qemu-system-i386 \
    -drive format=raw,file=${BOOT_IMG} \
    -drive format=raw,file=fat:rw:${DOORS} \
    -drive format=raw,file=fat:rw:${INST} 
