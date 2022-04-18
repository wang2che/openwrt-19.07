# Use the default kernel version if the Makefile doesn't override it

LINUX_RELEASE?=1

ifdef CONFIG_TESTING_KERNEL
  KERNEL_PATCHVER:=$(KERNEL_TESTING_PATCHVER)
endif

#openwrt-19.07 base
#LINUX_VERSION-4.14 = .275
#LINUX_KERNEL_HASH-4.14.275 = 100a9960fb2d8e079c9feeef640715a7fb749ed728a57e427f9e2443212e58f9

#linux-4.14.78-1.0.0_ga
#LINUX_VERSION-4.14 = .78
#LINUX_KERNEL_HASH-4.14.78 = 310fc81c0e314a85e30d224bb324b891c5de836b1903d94870d74b5131d83daf

#linux-4.14.98-2.0.0_ga
#LINUX_VERSION-4.14 = .98
#LINUX_KERNEL_HASH-4.14.98 = 0371d0f80be78a4394602732fccc14bff84fbd791b9c9c1d67bf4c03c293fda1

#linux-4.14.98-2.3.0 genvict
LINUX_VERSION-4.14 = .98
LINUX_KERNEL_HASH-4.14.98 = 735ba46418048e49f6a74941a932fabe34fa8ef0d4026089509fa10542fdc124

remove_uri_prefix=$(subst git://,,$(subst http://,,$(subst https://,,$(1))))
sanitize_uri=$(call qstrip,$(subst @,_,$(subst :,_,$(subst .,_,$(subst -,_,$(subst /,_,$(1)))))))

ifneq ($(call qstrip,$(CONFIG_KERNEL_GIT_CLONE_URI)),)
  LINUX_VERSION:=$(call sanitize_uri,$(call remove_uri_prefix,$(CONFIG_KERNEL_GIT_CLONE_URI)))
  ifeq ($(call qstrip,$(CONFIG_KERNEL_GIT_REF)),)
    CONFIG_KERNEL_GIT_REF:=HEAD
  endif
  LINUX_VERSION:=$(LINUX_VERSION)-$(call sanitize_uri,$(CONFIG_KERNEL_GIT_REF))
else
ifdef KERNEL_PATCHVER
  LINUX_VERSION:=$(KERNEL_PATCHVER)$(strip $(LINUX_VERSION-$(KERNEL_PATCHVER)))
endif
ifdef KERNEL_TESTING_PATCHVER
  LINUX_TESTING_VERSION:=$(KERNEL_TESTING_PATCHVER)$(strip $(LINUX_VERSION-$(KERNEL_TESTING_PATCHVER)))
endif
endif

split_version=$(subst ., ,$(1))
merge_version=$(subst $(space),.,$(1))
KERNEL_BASE=$(firstword $(subst -, ,$(LINUX_VERSION)))
KERNEL=$(call merge_version,$(wordlist 1,2,$(call split_version,$(KERNEL_BASE))))
KERNEL_PATCHVER ?= $(KERNEL)

# disable the md5sum check for unknown kernel versions
LINUX_KERNEL_HASH:=$(LINUX_KERNEL_HASH-$(strip $(LINUX_VERSION)))
LINUX_KERNEL_HASH?=x
