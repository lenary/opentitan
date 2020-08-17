// Copyright lowRISC contributors.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

#ifndef OPENTITAN_SW_DEVICE_LIB_RUNTIME_PMP_H_
#define OPENTITAN_SW_DEVICE_LIB_RUNTIME_PMP_H_

#include <stdbool.h>
#include <stdint.h>

// TODO - NOTE: PMP is only configurable in the M-mode!

/**
 * Attribute for functions which return errors that must be acknowledged.
 *
 * This attribute must be used to mark all PMP API that return an error value of
 * some kind, to ensure that callers do not accidentally drop the error on the
 * ground.
 */
#define PMP_WARN_UNUSED_RESULT __attribute__((warn_unused_result))

/**
 * RV32 PMP CSR definitions.
 *
 * The values for these defines have been taken from the:
 * Volume II: RISC-V Privileged Architectures V20190608-Priv-MSU-Ratified,
 * 3.6.1 Physical Memory Protection CSRs.
 */
#define PMP_REGIONS_NUM 16
#define PMP_CFG_MULTIREG_COUNT 4
#define PMP_ADDR_REG_COUNT 16

/**
 * RVISC-V PMP address matching modes.
 *
 * PMP address matching modes, how protected regions are formed and
 * implementation defined granularity information is described in:
 * "Volume II: RISC-V Privileged Architectures V20190608-Priv-MSU-Ratified",
 * "3.6.1 Physical Memory Protection CSRs", "Address Matching".
 *
 * PMP regions matching logic and prioritisation is described in the
 * "Priority and Matching Logic" paragraph of the section "3.6.1".
 *
 * Note that granularity is implementation defined (default G=0, making the
 * minimal PMP region size as 2^(G+2) = 4 bytes), and with G ≥ 1, the
 * `kPmpRegionAddrMatchingModeNa4` is not available. The granularity also
 * determined the minimal region size in `kPmpRegionAddrMatchingModeTor` and
 * `kPmpRegionAddrMatchingModeNapot` modes.
 */
typedef enum pmp_region_addr_matching_mode {
  /**
   * PMP address matching is disabled for this region.
   *
   * Note that when pmpcfgN is set to `kPmpRegionAddrMatchingModeOff`, but
   * pmpcfgN+1 is set to `kPmpRegionAddrMatchingModeTor`, then pmpaddrN will be
   * still used as the bottom range address for the TOR mode address matching.
   */
  kPmpRegionAddrMatchingModeOff = 0,

  /**
   * PMP Top Of the Range (TOR) address matching.
   *
   * When pmpcfgN is set to `kPmpRegionAddrMatchingModeTor`, the protected
   * address range is formed from the pmpaddrN-1 and pmpaddrN registers. Note
   * that the combination of pmpcfgN set to `kPmpRegionAddrMatchingModeTor` and
   * pmpcfgN-1 set to `kPmpRegionAddrMatchingModeNapot` does not form any
   * meaningful range for TOR addressing mode. pmpcfgN TOR mode will take the
   * bottom of the range pmpaddrN-1 address as is, without decoding the NAPOT
   * address.
   */
  kPmpRegionAddrMatchingModeTor,

  /**
   * PMP Naturally aligned four-byte region (NA4) address matching.
   *
   * When pmpcfgN is set to `kPmpRegionAddrMatchingModeNa4` then the protected
   * region range is formed from pmpaddrN and extends to the next 4 bytes.
   */
  kPmpRegionAddrMatchingModeNa4,

  /**
   * PMP Naturally aligned power-of-two region (NAPOT), ≥ 8 bytes.
   *
   * When pmpcfgN is set to `kPmpRegionAddrMatchingModeNa4` then the protected
   * region range is formed from a single encoded pmpaddrN address. NAPOT
   * ranges make use of the low-order bits of the associated address register
   * to encode the size of the range, please see the table "3.10" of the
   * "Address Matching" section of the specification mentioned above.
   *
   * Example: NAPOT encoded address of yyyy011 describes the
   * yyyy00000 .. yyyy11111 range. The index of the first "0", determines the
   * range size ( 2^(3 + index) ).
   */
  kPmpRegionAddrMatchingModeNapot,
} pmp_region_addr_matching_mode_t;

/**
 * PMP region access permissions.
 *
 * PMP permissions are described in:
 * "Volume II: RISC-V Privileged Architectures V20190608-Priv-MSU-Ratified",
 * "3.6.1 Physical Memory Protection CSRs", "Locking and Privilege Mode".
 *
 * Note that "The combination R=0 and W=1 is reserved for future use".
 *
 * When a region is configured with `kPmpRegionLockUnlocked` then these
 * permissions only apply to RISC-V "U" and "S" privilege modes, and have no
 * effect in the "M" mode.
 */
typedef enum pmp_region_permissions {
  kPmpRegionPermissionsNone = 0,         /**< No access permitted .*/
  kPmpRegionPermissionsReadOnly,         /**< Only read access. */
  kPmpRegionPermissionsExecuteOnly,      /**< Only execute access. */
  kPmpRegionPermissionsReadExecute,      /**< Read and execute access. */
  kPmpRegionPermissionsReadWrite,        /**< Read and write access. */
  kPmpRegionPermissionsReadWriteExecute, /**< Read, write and execute access. */
} pmp_region_permissions_t;

/**
 * PMP region locking.
 *
 * PMP locking is described in:
 * "Volume II: RISC-V Privileged Architectures V20190608-Priv-MSU-Ratified",
 * "3.6.1 Physical Memory Protection CSRs", "Locking and Privilege Mode".
 *
 * When a region is configured with `kPmpRegionLockLocked`, this region cannot
 * be accessed even by the machine "M" privilege code, and can be only unlocked
 * by the system reset. Additionally it also forces the
 * `pmp_region_permissions_t` access restrictions for the corresponding region
 * in machine "M" mode, which otherwise are ignored.
 */
typedef enum pmp_region_lock {
  kPmpRegionLockUnlocked = 0, /**< The region is unlocked. */
  kPmpRegionLockLocked,       /**< The region is locked. */
} pmp_region_lock_t;

/**
 * PMP region index is used to identify one of `PMP_REGIONS_NUM` PMP regions.
 */
typedef uint32_t pmp_region_index_t;

/**
 * PMP region address that forms a protected address range.
 */
typedef uint32_t pmp_region_address_t;

/**
 * PMP region configuration information.
 */
typedef struct pmp_region_config {
  pmp_region_lock_t lock; /**< Region lock for "M" privilege mode.*/
  pmp_region_permissions_t permissions; /**< Region access permissions. */
} pmp_region_config_t;

/**
 * PMP generic status codes.
 *
 * These error codes can be used by any function. However if a function
 * requires additional status codes, it must define a set of status codes to
 * be used exclusively by such function.
 */
typedef enum pmp_result {
  kPmpOk = 0, /**< Success. */
  kPmpError,  /**< General error. */
  kPmpBadArg, /**< Input parameter is not valid. */
} pmp_result_t;

/**
 * PMP region access and address configuration result.
 */
typedef enum pmp_region_configure_result {
  kPmpRegionConfigureOk = kPmpOk,
  kPmpRegionConfigureError = kPmpError,
  kPmpRegionConfigureBadArg = kPmpBadArg,
  /**
   * The requested region is invalid.
   */
  kPmpRegionConfigureBadRegion,
  /**
   * The requested region was not configured correctly. From the "Volume II:
   * RISC-V Privileged Architectures V20190608-Priv-MSU-Ratified":
   * "Implementations will not raise an exception on writes of unsupported
   * values to a WARL field. Implementations can return any legal value on the
   * read of a WARL field when the last write was of an illegal value, but the
   * legal value returned should deterministically depend on the illegal
   * written value and the value of the field prior to the write"
   */
  kPmpRegionConfigureWarlMismatch,
} pmp_region_configure_result_t;

/**
 * TODO
 *
 * TODO
 * @param region
 * @param address
 * @return `pmp_region_configure_result_t`.
 */
PMP_WARN_UNUSED_RESULT
pmp_region_configure_result_t pmp_region_configure_off(
  pmp_region_index_t region, pmp_region_address_t address);

/**
 * TODO
 *
 * TODO
 * @param region
 * @param config
 * @param address
 * @return `pmp_region_configure_result_t`.
 */
PMP_WARN_UNUSED_RESULT
pmp_region_configure_result_t pmp_region_configure_na4(
  pmp_region_index_t region, const pmp_region_config_t *config,
  pmp_region_address_t address);

/**
 * TODO
 *
 * TODO
 * @param region
 * @param config
 * @param address
 * @return `pmp_region_configure_result_t`.
 */
PMP_WARN_UNUSED_RESULT
pmp_region_configure_result_t pmp_region_configure_napot(
  pmp_region_index_t region, const pmp_region_config_t *config,
  pmp_region_address_t address);

/**
 * TODO
 *
 * TODO
 * @param region
 * @param config
 * @param address
 * @return `pmp_region_configure_result_t`.
 */
PMP_WARN_UNUSED_RESULT
pmp_region_configure_result_t pmp_region_configure_tor(
  pmp_region_index_t region, const pmp_region_config_t *config,
  pmp_region_address_t address_start, pmp_region_address_t address_end);

/**
 * PMP region configuration and address get results.
 */
typedef enum pmp_region_get_result {
  kPmpRegionGetOk = kPmpOk,
  kPmpRegionGetError = kPmpError,
  kPmpRegionGetBadArg = kPmpBadArg,
  kPmpRegionGetBadRegion, /**< TODO. */
} pmp_region_get_result_t;

/**
 * TODO
 *
 * TODO
 * @param region
 * @param config
 * @return `pmp_region_get_result_t`.
 */
PMP_WARN_UNUSED_RESULT
pmp_region_get_result_t pmp_region_config_get(pmp_region_index_t region,
                                              pmp_region_config_t *config);

// TODO - inline helpers to construct, NAPOT and TOR addresses! To what extent
//        we need it? Without bitmanip it seems slightly inefficient.
// TODO - Does PMP need to be enabled somehwere? NO!
// TODO - Granularity check function? Hardcoded! It can be done
//        programatically, but there are some caveats?

#endif  // OPENTITAN_SW_DEVICE_LIB_RUNTIME_PMP_H_
