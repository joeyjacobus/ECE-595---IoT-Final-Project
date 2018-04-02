
qemu-system-arm \
-M versatilepb \
-kernel output/images/zImage \
-dtb output/images/versatile-pb.dtb \
-drive file=output/images/rootfs.ext2,if=scsi,format=raw -append "root=/dev/sda console=ttyAMA0,115200" \
-serial stdio \
-net nic,model=rtl8139 -net user,hostfwd=tcp:127.0.0.1:2222-:22
#-redir tcp:2222:22

