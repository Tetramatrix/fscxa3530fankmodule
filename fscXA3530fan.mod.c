/*
    Fan control kernel module for FSC Amilo XA3530 computers
    Copyright (C) 2011-2014 Chi Hoang

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
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

static const struct modversion_info ____versions[]
__used
__attribute__((section("__versions"))) = {
	{ 0x63212168, "module_layout" },
	{ 0xfd6293c2, "boot_tvec_bases" },
	{ 0xf4e6ea8d, "device_destroy" },
	{ 0x8c03d20c, "destroy_workqueue" },
	{ 0x42160169, "flush_workqueue" },
	{ 0x6f0036d9, "del_timer_sync" },
	{ 0xdce87da5, "class_destroy" },
	{ 0x7485e15e, "unregister_chrdev_region" },
	{ 0x3142c4e1, "cdev_del" },
	{ 0xee7d9037, "device_create" },
	{ 0xbd859cd, "__class_create" },
	{ 0xb70f26e9, "cdev_add" },
	{ 0x89821cac, "cdev_alloc" },
	{ 0x29537c9e, "alloc_chrdev_region" },
	{ 0x91715312, "sprintf" },
	{ 0xc3aaf0a9, "__put_user_1" },
	{ 0x88a193, "__alloc_workqueue_key" },
	{ 0x43b0c9c3, "preempt_schedule" },
	{ 0x4c4fef19, "kernel_stack" },
	{ 0x2242d163, "module_put" },
	{ 0x71dc0277, "queue_delayed_work" },
	{ 0x27e1a049, "printk" },
	{ 0xba2d8594, "ec_read" },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";

