# Usage with Vitis IDE:
# In Vitis IDE create a Single Application Debug launch configuration,
# change the debug type to 'Attach to running target' and provide this 
# tcl script in 'Execute Script' option.
# Path of this script: C:\projects\2189\Adc_DVGA_config\lpbck_intr_wrkspce\Interrupt_DMA_PSPL_system\_ide\scripts\systemdebugger_interrupt_dma_pspl_system_standalone.tcl
# 
# 
# Usage with xsct:
# To debug using xsct, launch xsct and run below command
# source C:\projects\2189\Adc_DVGA_config\lpbck_intr_wrkspce\Interrupt_DMA_PSPL_system\_ide\scripts\systemdebugger_interrupt_dma_pspl_system_standalone.tcl
# 
connect -url tcp:127.0.0.1:3121
source E:/Softwares/Xilinx_2022_2_sys_Files/Vitis/2022.2/scripts/vitis/util/zynqmp_utils.tcl
targets -set -nocase -filter {name =~"APU*"}
rst -system
after 3000
targets -set -filter {jtag_cable_name =~ "Digilent JTAG-HS3 210299ABB7ED" && level==0 && jtag_device_ctx=="jsn-JTAG-HS3-210299ABB7ED-04720093-0"}
fpga -file C:/projects/2189/Adc_DVGA_config/lpbck_intr_wrkspce/Interrupt_DMA_PSPL/_ide/bitstream/DMA_testing.bit
targets -set -nocase -filter {name =~"APU*"}
loadhw -hw C:/projects/2189/Adc_DVGA_config/lpbck_intr_wrkspce/DMA_testing/export/DMA_testing/hw/DMA_testing.xsa -mem-ranges [list {0x80000000 0xbfffffff} {0x400000000 0x5ffffffff} {0x1000000000 0x7fffffffff}] -regs
configparams force-mem-access 1
targets -set -nocase -filter {name =~"APU*"}
set mode [expr [mrd -value 0xFF5E0200] & 0xf]
targets -set -nocase -filter {name =~ "*A53*#0"}
rst -processor
dow C:/projects/2189/Adc_DVGA_config/lpbck_intr_wrkspce/DMA_testing/export/DMA_testing/sw/DMA_testing/boot/fsbl.elf
set bp_0_33_fsbl_bp [bpadd -addr &XFsbl_Exit]
con -block -timeout 60
bpremove $bp_0_33_fsbl_bp
targets -set -nocase -filter {name =~ "*A53*#0"}
rst -processor
dow C:/projects/2189/Adc_DVGA_config/lpbck_intr_wrkspce/Interrupt_DMA_PSPL/Debug/Interrupt_DMA_PSPL.elf
configparams force-mem-access 0
targets -set -nocase -filter {name =~ "*A53*#0"}
con
