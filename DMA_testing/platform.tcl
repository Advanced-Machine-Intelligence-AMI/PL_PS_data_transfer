# 
# Usage: To re-create this platform project launch xsct with below options.
# xsct C:\projects\2189\Adc_DVGA_config\lpbck_intr_wrkspce\DMA_testing\platform.tcl
# 
# OR launch xsct and run below command.
# source C:\projects\2189\Adc_DVGA_config\lpbck_intr_wrkspce\DMA_testing\platform.tcl
# 
# To create the platform in a different location, modify the -out option of "platform create" command.
# -out option specifies the output directory of the platform project.

platform create -name {DMA_testing}\
-hw {C:\Users\KrishnaVamshi(ELD-IN\Downloads\DMA_testing.xsa}\
-arch {64-bit} -fsbl-target {psu_cortexa53_0} -out {C:/projects/2189/Adc_DVGA_config/lpbck_intr_wrkspce}

platform write
domain create -name {standalone_psu_cortexa53_0} -display-name {standalone_psu_cortexa53_0} -os {standalone} -proc {psu_cortexa53_0} -runtime {cpp} -arch {64-bit} -support-app {hello_world}
platform generate -domains 
platform active {DMA_testing}
domain active {zynqmp_fsbl}
domain active {zynqmp_pmufw}
domain active {standalone_psu_cortexa53_0}
platform generate -quick
platform generate
