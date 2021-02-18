# Copyright 2019-2020, Collabora, Ltd.
# SPDX-License-Identifier: BSL-1.0

driver_libs = []
driver_deps = []

if libusb.found()
	driver_deps += [libusb]
endif

if libjpeg.found()
	driver_deps += [libjpeg]
endif

if 'dummy' in drivers
	driver_libs += [lib_drv_dummy]
endif

if 'hdk' in drivers
	driver_libs += [lib_drv_hdk]
endif

if 'hydra' in drivers
	driver_libs += [lib_drv_hydra]
endif

if 'ns' in drivers
	driver_libs += [lib_drv_ns]
endif

if 'ohmd' in drivers
	driver_libs += [lib_drv_ohmd]
endif

if 'handtracking' in drivers
	driver_libs += [lib_drv_ht]
endif

if 'psmv' in drivers
	driver_libs += [lib_drv_psmv]
endif

if 'psvr' in drivers
	driver_libs += [lib_drv_psvr]
endif

if 'rs' in drivers
	driver_libs += [lib_drv_rs]
	driver_deps += [rs]
endif

if 'v4l2' in drivers
	driver_libs += [lib_drv_v4l2]
endif

if 'vf' in drivers
	driver_libs += [lib_drv_vf]
endif

if 'vive' in drivers
	driver_libs += [lib_drv_vive]
endif

if 'survive' in drivers
	driver_libs += [lib_drv_survive]
endif

if 'daydream' in drivers
	driver_libs += [lib_drv_daydream]
endif

if 'arduino' in drivers
	driver_libs += [lib_drv_arduino]
endif

if 'qwerty' in drivers
	driver_libs += [lib_drv_qwerty]
endif

if 'remote' in drivers
	driver_libs += [lib_drv_remote]
endif

driver_libs += [lib_drv_multi]

subdir('common')
subdir('openxr')
subdir('cli')

if get_option('gui').enabled() or (get_option('gui').auto() and sdl2.found())
	subdir('gui')
endif

if get_option('service')
	subdir('service')
	subdir('ctl')
endif

if get_option('steamvr_plugin')
	subdir('steamvr_drv')
endif
