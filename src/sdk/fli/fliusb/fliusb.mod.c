#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

MODULE_INFO(vermagic, VERMAGIC_STRING);

struct module __this_module
__attribute__((section(".gnu.linkonce.this_module"))) = {
 .name = KBUILD_MODNAME,
 .init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
 .exit = cleanup_module,
#endif
 .arch = MODULE_ARCH_INIT,
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";

MODULE_ALIAS("usb:v0F18p0002d*dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v0F18p0005d*dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v0F18p0006d*dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v0F18p0007d*dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v0F18p000Ad*dc*dsc*dp*ic*isc*ip*");

MODULE_INFO(srcversion, "7BB174432F5A9121E633FDC");
