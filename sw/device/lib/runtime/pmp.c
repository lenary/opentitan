// Copyright lowRISC contributors.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

#include "sw/device/lib/runtime/pmp.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "sw/device/lib/base/bitfield.h"
#include "sw/device/lib/base/stdasm.h"

// TODO relevant bits of the spec

#define PMP_CFG_CSR_R 0
#define PMP_CFG_CSR_W 1
#define PMP_CFG_CSR_X 2
#define PMP_CFG_CSR_A 3
#define PMP_CFG_CSR_L 7

#define PMP_CFG_CSR_MODE_OFF 0
#define PMP_CFG_CSR_MODE_TOR 1
#define PMP_CFG_CSR_MODE_NA4 2
#define PMP_CFG_CSR_MODE_NAPOT 3
#define PMP_CFG_CSR_MODE_MASK 0x3

#define PMP_CFG_FIELDS_PER_REG 4
#define PMP_CFG_FIELD_WIDTH 8
#define PMP_CFG_FIELD_MASK 0xffff

// TODO - spec reference
#define PMP_GRANULARITY_IBEX 0
#define PMP_ADDRESS_MIN_ALIGNMENT 4
#define PMP_ADDRESS_ALIGNMENT (4 << PMP_GRANULARITY_IBEX)
#define PMP_ADDRESS_ALIGNMENT_MASK (PMP_ADDRESS_ALIGNMENT - 1)
#define PMP_ADDRESS_ALIGNMENT_INVERTED_MASK (~PMP_ADDRESS_ALIGNMENT_MASK)
#define PMP_ADDRESS_SHIFT 2

#define PMP_CSR_WRITE(csr, var) asm volatile("csrw " #csr ", %0;" : : "r"(var))

#define PMP_CSR_READ(csr, var) asm volatile("csrr %0, " #csr ";" : "=r"(var) :)

#define PMP_CSR_RW(access, csr, var)       \
  do {                                     \
    if (access == kPmpCsrAccessTypeRead) { \
      PMP_CSR_READ(csr, var);              \
    } else {                               \
      PMP_CSR_WRITE(csr, var);             \
    }                                      \
  } while (false)

typedef enum pmp_csr_access_type {
  kPmpCsrAccessTypeRead = 0,
  kPmpCsrAccessTypeWrite,
} pmp_csr_access_type_t;

static size_t pmp_csr_field_index(pmp_region_index_t region,
                                  uint32_t fields_per_reg,
                                  uint32_t field_width) {
  size_t field_index = region % fields_per_reg;
  return field_index * field_width;
}

static bool pmp_cfg_permissions_set(
  pmp_region_permissions_t perm, uint32_t *bitfield) {
  switch (perm) {
    case kPmpRegionPermissionsNone:
      // No access is allowed.
      break;
    case kPmpRegionPermissionsReadOnly:
      *bitfield = bitfield_bit32_write(*bitfield, PMP_CFG_CSR_R, true);
      break;
    case kPmpRegionPermissionsExecuteOnly:
      *bitfield = bitfield_bit32_write(*bitfield, PMP_CFG_CSR_X, true);
      break;
    case kPmpRegionPermissionsReadExecute:
      *bitfield = bitfield_bit32_write(*bitfield, PMP_CFG_CSR_R, true);
      *bitfield = bitfield_bit32_write(*bitfield, PMP_CFG_CSR_X, true);
      break;
    case kPmpRegionPermissionsReadWrite:
      *bitfield = bitfield_bit32_write(*bitfield, PMP_CFG_CSR_R, true);
      *bitfield = bitfield_bit32_write(*bitfield, PMP_CFG_CSR_W, true);
      break;
    case kPmpRegionPermissionsReadWriteExecute:
      *bitfield = bitfield_bit32_write(*bitfield, PMP_CFG_CSR_R, true);
      *bitfield = bitfield_bit32_write(*bitfield, PMP_CFG_CSR_W, true);
      *bitfield = bitfield_bit32_write(*bitfield, PMP_CFG_CSR_X, true);
      break;
    default:
      return false;
  }

  return true;
}

static bool pmp_cfg_mode_set(
  pmp_region_addr_matching_mode_t mode, uint32_t *bitfield) {
  bitfield_field32_t mode_field = {
      .mask = PMP_CFG_CSR_MODE_MASK, .index = PMP_CFG_CSR_A,
  };

  switch (mode) {
    case kPmpRegionAddrMatchingModeOff:
      *bitfield =
          bitfield_field32_write(*bitfield, mode_field, PMP_CFG_CSR_MODE_OFF);
      break;
    case kPmpRegionAddrMatchingModeTor:
      *bitfield =
          bitfield_field32_write(*bitfield, mode_field, PMP_CFG_CSR_MODE_TOR);
      break;
    case kPmpRegionAddrMatchingModeNa4:
      *bitfield =
          bitfield_field32_write(*bitfield, mode_field, PMP_CFG_CSR_MODE_NA4);
      break;
    case kPmpRegionAddrMatchingModeNapot:
      *bitfield =
          bitfield_field32_write(*bitfield, mode_field, PMP_CFG_CSR_MODE_NAPOT);
    default:
      return false;
  }

  return true;
}

static void pmp_cfg_mode_lock_set(pmp_region_lock_t lock, uint32_t *bitfield) {
  if (lock == kPmpRegionLockLocked) {
    *bitfield = bitfield_bit32_write(*bitfield, PMP_CFG_CSR_L, true);
  } else {
    *bitfield = bitfield_bit32_write(*bitfield, PMP_CFG_CSR_L, false);
  }
}

static bool pmp_address_aligned(pmp_region_address_t address) {
  return address == (address & PMP_ADDRESS_ALIGNMENT_INVERTED_MASK);
}

static pmp_region_configure_result_t pmp_cfg_csr_rw(
    pmp_region_index_t region, pmp_csr_access_type_t access, uint32_t *value) {
  switch (region) {
    case 0:
    case 1:
    case 2:
    case 3:
      PMP_CSR_RW(access, pmpcfg0, *value);
      break;
    case 4:
    case 5:
    case 6:
    case 7:
      PMP_CSR_RW(access, pmpcfg1, *value);
      break;
    case 8:
    case 9:
    case 10:
    case 11:
      PMP_CSR_RW(access, pmpcfg2, *value);
      break;
    case 12:
    case 13:
    case 14:
    case 15:
      PMP_CSR_RW(access, pmpcfg3, *value);
      break;
    default:
      return kPmpRegionConfigureError;
  }

  return kPmpRegionConfigureOk;
}

static bool pmp_addr_csr_rw(pmp_region_index_t region,
                            pmp_csr_access_type_t access, uint32_t *value) {
  switch (region) {
    case 0:
      PMP_CSR_RW(access, pmpaddr0, *value);
      break;
    case 1:
      PMP_CSR_RW(access, pmpaddr1, *value);
      break;
    case 2:
      PMP_CSR_RW(access, pmpaddr2, *value);
      break;
    case 3:
      PMP_CSR_RW(access, pmpaddr3, *value);
      break;
    case 4:
      PMP_CSR_RW(access, pmpaddr4, *value);
      break;
    case 5:
      PMP_CSR_RW(access, pmpaddr5, *value);
      break;
    case 6:
      PMP_CSR_RW(access, pmpaddr6, *value);
      break;
    case 7:
      PMP_CSR_RW(access, pmpaddr7, *value);
      break;
    case 8:
      PMP_CSR_RW(access, pmpaddr8, *value);
      break;
    case 9:
      PMP_CSR_RW(access, pmpaddr9, *value);
      break;
    case 10:
      PMP_CSR_RW(access, pmpaddr10, *value);
      break;
    case 11:
      PMP_CSR_RW(access, pmpaddr11, *value);
      break;
    case 12:
      PMP_CSR_RW(access, pmpaddr12, *value);
      break;
    case 13:
      PMP_CSR_RW(access, pmpaddr13, *value);
      break;
    case 14:
      PMP_CSR_RW(access, pmpaddr14, *value);
      break;
    case 15:
      PMP_CSR_RW(access, pmpaddr15, *value);
      break;
    default:
      return false;
  }

  return true;
}

pmp_region_configure_result_t pmp_region_address_set(
  pmp_region_index_t region, pmp_region_address_t address) {
  // TODO - spec reference
  pmp_region_address_t shifted_address = address >> PMP_ADDRESS_SHIFT;

  if (!pmp_addr_csr_rw(region, kPmpCsrAccessTypeWrite, &shifted_address)) {
    return kPmpRegionConfigureError;
  }

  pmp_region_address_t addr_csr_after_write;
  if (!pmp_addr_csr_rw(region, kPmpCsrAccessTypeRead, &addr_csr_after_write)) {
    return kPmpRegionConfigureError;
  }

  if (shifted_address != addr_csr_after_write) {
    return kPmpRegionConfigureWarlMismatch;
  }

  return kPmpRegionConfigureOk;
}

static pmp_region_configure_result_t pmp_region_configure(
  pmp_region_index_t region, uint32_t field_value) {
  uint32_t cfg_csr_original;
  if (!pmp_cfg_csr_rw(region, kPmpCsrAccessTypeRead, &cfg_csr_original)) {
    return kPmpRegionConfigureError;
  }

  size_t field_index =
      pmp_csr_field_index(region, PMP_CFG_FIELDS_PER_REG, PMP_CFG_FIELD_WIDTH);

  uint32_t cfg_csr_to_write = bitfield_field32_write(
      cfg_csr_original,
      (bitfield_field32_t){
          .mask = PMP_CFG_FIELD_MASK, .index = field_index,
      },
      field_value);

  if (!pmp_cfg_csr_rw(region, kPmpCsrAccessTypeWrite, &cfg_csr_to_write)) {
    return kPmpRegionConfigureError;
  }

  uint32_t cfg_csr_after_write;
  if (!pmp_cfg_csr_rw(region, kPmpCsrAccessTypeRead, &cfg_csr_after_write)) {
    return kPmpRegionConfigureError;
  }

  if (cfg_csr_to_write != cfg_csr_after_write) {
    return kPmpRegionConfigureWarlMismatch;
  }

  return kPmpRegionConfigureOk;
}

pmp_region_configure_result_t pmp_region_configure_off(
  pmp_region_index_t region, pmp_region_address_t address) {
  if (region >= PMP_REGIONS_NUM) {
    return kPmpRegionConfigureBadRegion;
  }

  if (!pmp_address_aligned(address)) {
    // TODO - correct return code.
  }

  // TODO - lock check

  // Address registers must be written prior to the configuration registers to
  // ensure that they are not locked.
  pmp_region_configure_result_t result =
    pmp_region_address_set(region, address);
  if (result != kPmpRegionConfigureOk) {
    return result;
  }

  // TODO - 0, but set mode through the function.
  result = pmp_region_configure(region, 0);
  if (result != kPmpRegionConfigureOk) {
    return result;
  }

  // TODO - address setting.

  return kPmpRegionConfigureOk;
}

pmp_region_configure_result_t pmp_region_configure_na4(
  pmp_region_index_t region, const pmp_region_config_t *config,
  pmp_region_address_t address) {
  if (config == NULL) {
    return kPmpRegionConfigureBadArg;
  }

  if (region >= PMP_REGIONS_NUM) {
    return kPmpRegionConfigureBadRegion;
  }

  if (PMP_GRANULARITY_IBEX > 0) {
    // TODO - appropriate error code.
  }

  // TODO - lock check

  if (!pmp_address_aligned(address)) {
    // TODO - correct return code.
  }

  uint32_t field_value = 0;
  if (!pmp_cfg_permissions_set(config->permissions, &field_value)) {
    return kPmpRegionConfigureError;
  }

  if (!pmp_cfg_mode_set(kPmpRegionAddrMatchingModeNa4, &field_value)) {
    return kPmpRegionConfigureError;
  }

  pmp_cfg_mode_lock_set(config->lock, &field_value);

  // Address registers must be written prior to the configuration registers to
  // ensure that they are not locked.
  pmp_region_configure_result_t result =
    pmp_region_address_set(region, address);
  if (result != kPmpRegionConfigureOk) {
    return result;
  }

  result = pmp_region_configure(region, field_value);
  if (result != kPmpRegionConfigureOk) {
    return result;
  }

  return kPmpRegionConfigureOk;
}

// TODO - NAPOT configure
// TODO - TOR configure

pmp_region_get_result_t pmp_region_config_get(pmp_region_index_t region,
                                              pmp_region_config_t *config) {
  if (config == NULL) {
    return kPmpRegionGetBadArg;
  }

  if (region >= PMP_REGIONS_NUM) {
    return kPmpRegionGetBadRegion;
  }

  // TODO

  return kPmpRegionGetError;
}
