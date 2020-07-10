#include <string.h>
#include "os.h"
#include "os_io_seproxyhal.h"

#include "algo_ui.h"
#include "algo_tx.h"
#include "algo_addr.h"
#include "algo_keys.h"
#include "base64.h"
#include "glyphs.h"

static char *
u64str(uint64_t v)
{
  static char buf[24];

  char *p = &buf[sizeof(buf)];
  *(--p) = '\0';

  if (v == 0) {
    *(--p) = '0';
    return p;
  }

  while (v > 0) {
    *(--p) = '0' + (v % 10);
    v = v/10;
  }

  return p;
}

static int
all_zero_key(uint8_t *buf)
{
  for (int i = 0; i < 32; i++) {
    if (buf[i] != 0) {
      return 0;
    }
  }

  return 1;
}

static int step_txn_type() {
  switch (current_txn.type) {
  case PAYMENT:
    ui_text_put("Payment");
    break;

  case KEYREG:
    ui_text_put("Key reg");
    break;

  case ASSET_XFER:
    ui_text_put("Asset xfer");
    break;

  case ASSET_FREEZE:
    ui_text_put("Asset freeze");
    break;

  case ASSET_CONFIG:
    ui_text_put("Asset config");
    break;

  default:
    ui_text_put("Unknown");
  }
  return 1;
}

static int step_sender() {
  uint8_t publicKey[32];
  cx_ecfp_private_key_t privateKey;
  algorand_key_derive(current_txn.accountId, &privateKey);
  algorand_public_key(&privateKey, publicKey);
  if (os_memcmp(publicKey, current_txn.sender, sizeof(current_txn.sender)) == 0) {
    ux_approve_txn();
    return 0;
  }

  char checksummed[65];
  checksummed_addr(current_txn.sender, checksummed);
  ui_text_put(checksummed);
  return 1;
}

static int step_fee() {
  ui_text_put(u64str(current_txn.fee));
  return 1;
}

static int step_firstvalid() {
  ui_text_put(u64str(current_txn.firstValid));
  return 1;
}

static int step_lastvalid() {
  ui_text_put(u64str(current_txn.lastValid));
  return 1;
}

static const char* default_genesisID = "mainnet-v1.0";
static const uint8_t default_genesisHash[] = {
  0xc0, 0x61, 0xc4, 0xd8, 0xfc, 0x1d, 0xbd, 0xde, 0xd2, 0xd7, 0x60, 0x4b, 0xe4, 0x56, 0x8e, 0x3f, 0x6d, 0x4, 0x19, 0x87, 0xac, 0x37, 0xbd, 0xe4, 0xb6, 0x20, 0xb5, 0xab, 0x39, 0x24, 0x8a, 0xdf,
};

static int step_genesisID() {
  if (strncmp(current_txn.genesisID, default_genesisID, sizeof(current_txn.genesisID)) == 0) {
    ux_approve_txn();
    return 0;
  }

  if (current_txn.genesisID[0] == '\0') {
    ux_approve_txn();
    return 0;
  }

  ui_text_putn(current_txn.genesisID, sizeof(current_txn.genesisID));
  return 1;
}

static int step_genesisHash() {
  if (all_zero_key(current_txn.genesisHash)) {
    ux_approve_txn();
    return 0;
  }

  if (strncmp(current_txn.genesisID, default_genesisID, sizeof(current_txn.genesisID)) == 0 ||
      current_txn.genesisID[0] == '\0') {
    if (os_memcmp(current_txn.genesisHash, default_genesisHash, sizeof(current_txn.genesisHash)) == 0) {
      ux_approve_txn();
      return 0;
    }
  }

  char buf[45];
  base64_encode((const char*) current_txn.genesisHash, sizeof(current_txn.genesisHash), buf, sizeof(buf));
  ui_text_put(buf);
  return 1;
}

static int step_note() {
  if (current_txn.note_len == 0) {
    ux_approve_txn();
    return 0;
  }

  char buf[16];
  snprintf(buf, sizeof(buf), "%d bytes", current_txn.note_len);
  ui_text_put(buf);
  return 1;
}

static int step_receiver() {
  char checksummed[65];
  checksummed_addr(current_txn.payment.receiver, checksummed);
  ui_text_put(checksummed);
  return 1;
}

static int step_amount() {
  ui_text_put(u64str(current_txn.payment.amount));
  return 1;
}

static int step_close() {
  if (all_zero_key(current_txn.payment.close)) {
    ux_approve_txn();
    return 0;
  }

  char checksummed[65];
  checksummed_addr(current_txn.payment.close, checksummed);
  ui_text_put(checksummed);
  return 1;
}

static int step_votepk() {
  char buf[45];
  base64_encode((const char*) current_txn.keyreg.votepk, sizeof(current_txn.keyreg.votepk), buf, sizeof(buf));
  ui_text_put(buf);
  return 1;
}

static int step_vrfpk() {
  char buf[45];
  base64_encode((const char*) current_txn.keyreg.vrfpk, sizeof(current_txn.keyreg.vrfpk), buf, sizeof(buf));
  ui_text_put(buf);
  return 1;
}

static int step_asset_xfer_id() {
  ui_text_put(u64str(current_txn.asset_xfer.id));
  return 1;
}

static int step_asset_xfer_amount() {
  ui_text_put(u64str(current_txn.asset_xfer.amount));
  return 1;
}

static int step_asset_xfer_sender() {
  if (all_zero_key(current_txn.asset_xfer.sender)) {
    ux_approve_txn();
    return 0;
  }

  char checksummed[65];
  checksummed_addr(current_txn.asset_xfer.sender, checksummed);
  ui_text_put(checksummed);
  return 1;
}

static int step_asset_xfer_receiver() {
  if (all_zero_key(current_txn.asset_xfer.receiver)) {
    ux_approve_txn();
    return 0;
  }

  char checksummed[65];
  checksummed_addr(current_txn.asset_xfer.receiver, checksummed);
  ui_text_put(checksummed);
  return 1;
}

static int step_asset_xfer_close() {
  if (all_zero_key(current_txn.asset_xfer.close)) {
    ux_approve_txn();
    return 0;
  }

  char checksummed[65];
  checksummed_addr(current_txn.asset_xfer.close, checksummed);
  ui_text_put(checksummed);
  return 1;
}

static int step_asset_freeze_id() {
  ui_text_put(u64str(current_txn.asset_freeze.id));
  return 1;
}

static int step_asset_freeze_account() {
  if (all_zero_key(current_txn.asset_freeze.account)) {
    ux_approve_txn();
    return 0;
  }

  char checksummed[65];
  checksummed_addr(current_txn.asset_freeze.account, checksummed);
  ui_text_put(checksummed);
  return 1;
}

static int step_asset_freeze_flag() {
  if (current_txn.asset_freeze.flag) {
    ui_text_put("Frozen");
  } else {
    ui_text_put("Unfrozen");
  }
  return 1;
}

static int step_asset_config_id() {
  if (current_txn.asset_config.id == 0) {
    ui_text_put("Create");
  } else {
    ui_text_put(u64str(current_txn.asset_config.id));
  }
  return 1;
}

static int step_asset_config_total() {
  if (current_txn.asset_config.id != 0 && current_txn.asset_config.params.total == 0) {
    ux_approve_txn();
    return 0;
  }

  ui_text_put(u64str(current_txn.asset_config.params.total));
  return 1;
}

static int step_asset_config_default_frozen() {
  if (current_txn.asset_config.id != 0 && current_txn.asset_config.params.default_frozen == 0) {
    ux_approve_txn();
    return 0;
  }

  if (current_txn.asset_config.params.default_frozen) {
    ui_text_put("Frozen");
  } else {
    ui_text_put("Unfrozen");
  }
  return 1;
}

static int step_asset_config_unitname() {
  if (current_txn.asset_config.params.unitname[0] == '\0') {
    ux_approve_txn();
    return 0;
  }

  ui_text_putn(current_txn.asset_config.params.unitname, sizeof(current_txn.asset_config.params.unitname));
  return 1;
}

static int step_asset_config_decimals() {
  if (current_txn.asset_config.params.decimals == 0) {
    ux_approve_txn();
    return 0;
  }

  ui_text_put(u64str(current_txn.asset_config.params.decimals));
  return 1;
}

static int step_asset_config_assetname() {
  if (current_txn.asset_config.params.assetname[0] == '\0') {
    ux_approve_txn();
    return 0;
  }

  ui_text_putn(current_txn.asset_config.params.assetname, sizeof(current_txn.asset_config.params.assetname));
  return 1;
}

static int step_asset_config_url() {
  if (current_txn.asset_config.params.url[0] == '\0') {
    ux_approve_txn();
    return 0;
  }

  ui_text_putn(current_txn.asset_config.params.url, sizeof(current_txn.asset_config.params.url));
  return 1;
}

static int step_asset_config_metadata_hash() {
  if (all_zero_key(current_txn.asset_config.params.metadata_hash)) {
    ux_approve_txn();
    return 0;
  }

  char buf[45];
  base64_encode((const char*) current_txn.asset_config.params.metadata_hash, sizeof(current_txn.asset_config.params.metadata_hash), buf, sizeof(buf));
  ui_text_put(buf);
  return 1;
}

static int step_asset_config_addr_helper(uint8_t *addr) {
  if (all_zero_key(addr)) {
    ui_text_put("Zero");
  } else {
    char checksummed[65];
    checksummed_addr(addr, checksummed);
    ui_text_put(checksummed);
  }
  return 1;
}

static int step_asset_config_manager() {
  return step_asset_config_addr_helper(current_txn.asset_config.params.manager);
}

static int step_asset_config_reserve() {
  return step_asset_config_addr_helper(current_txn.asset_config.params.reserve);
}

static int step_asset_config_freeze() {
  return step_asset_config_addr_helper(current_txn.asset_config.params.freeze);
}

static int step_asset_config_clawback() {
  return step_asset_config_addr_helper(current_txn.asset_config.params.clawback);
}

static unsigned int ux_last_step;

ALGO_UX_STEP_NOCB_INIT(ALL_TYPES, 0, bn,          step_txn_type(),    {"Txn type",     text});
ALGO_UX_STEP_NOCB_INIT(ALL_TYPES, 1, bnnn_paging, step_sender(),      {"Sender",       text});
ALGO_UX_STEP_NOCB_INIT(ALL_TYPES, 2, bn,          step_fee(),         {"Fee (uAlg)",   text});
// ALGO_UX_STEP_NOCB_INIT(ALL_TYPES, 3, bn,          step_firstvalid(),  {"First valid",  text});
// ALGO_UX_STEP_NOCB_INIT(ALL_TYPES, 4, bn,          step_lastvalid(),   {"Last valid",   text});
ALGO_UX_STEP_NOCB_INIT(ALL_TYPES, 5, bn,          step_genesisID(),   {"Genesis ID",   text});
ALGO_UX_STEP_NOCB_INIT(ALL_TYPES, 6, bnnn_paging, step_genesisHash(), {"Genesis hash", text});
ALGO_UX_STEP_NOCB_INIT(ALL_TYPES, 7, bn,          step_note(),        {"Note",         text});

ALGO_UX_STEP_NOCB_INIT(PAYMENT, 8,  bnnn_paging, step_receiver(), {"Receiver",      text});
ALGO_UX_STEP_NOCB_INIT(PAYMENT, 9,  bn,          step_amount(),   {"Amount (uAlg)", text});
ALGO_UX_STEP_NOCB_INIT(PAYMENT, 10, bnnn_paging, step_close(),    {"Close to",      text});

ALGO_UX_STEP_NOCB_INIT(KEYREG, 11, bnnn_paging, step_votepk(), {"Vote PK", text});
ALGO_UX_STEP_NOCB_INIT(KEYREG, 12, bnnn_paging, step_vrfpk(),  {"VRF PK",  text});

ALGO_UX_STEP_NOCB_INIT(ASSET_XFER, 13, bn,          step_asset_xfer_id(),       {"Asset ID",   text});
ALGO_UX_STEP_NOCB_INIT(ASSET_XFER, 14, bn,          step_asset_xfer_amount(),   {"Asset amt",   text});
ALGO_UX_STEP_NOCB_INIT(ASSET_XFER, 15, bnnn_paging, step_asset_xfer_sender(),   {"Asset src",   text});
ALGO_UX_STEP_NOCB_INIT(ASSET_XFER, 16, bnnn_paging, step_asset_xfer_receiver(), {"Asset dst",   text});
ALGO_UX_STEP_NOCB_INIT(ASSET_XFER, 17, bnnn_paging, step_asset_xfer_close(),    {"Asset close", text});

ALGO_UX_STEP_NOCB_INIT(ASSET_FREEZE, 18, bn,          step_asset_freeze_id(),      {"Asset ID",      text});
ALGO_UX_STEP_NOCB_INIT(ASSET_FREEZE, 19, bnnn_paging, step_asset_freeze_account(), {"Asset account", text});
ALGO_UX_STEP_NOCB_INIT(ASSET_FREEZE, 20, bn,          step_asset_freeze_flag(),    {"Freeze flag",   text});

ALGO_UX_STEP_NOCB_INIT(ASSET_CONFIG, 21, bn,          step_asset_config_id(),             {"Asset ID",       text});
ALGO_UX_STEP_NOCB_INIT(ASSET_CONFIG, 22, bn,          step_asset_config_total(),          {"Total units",    text});
ALGO_UX_STEP_NOCB_INIT(ASSET_CONFIG, 23, bn,          step_asset_config_default_frozen(), {"Default frozen", text});
ALGO_UX_STEP_NOCB_INIT(ASSET_CONFIG, 24, bnnn_paging, step_asset_config_unitname(),       {"Unit name",      text});
ALGO_UX_STEP_NOCB_INIT(ASSET_CONFIG, 25, bn,          step_asset_config_decimals(),       {"Decimals",       text});
ALGO_UX_STEP_NOCB_INIT(ASSET_CONFIG, 26, bnnn_paging, step_asset_config_assetname(),      {"Asset name",     text});
ALGO_UX_STEP_NOCB_INIT(ASSET_CONFIG, 27, bnnn_paging, step_asset_config_url(),            {"URL",            text});
ALGO_UX_STEP_NOCB_INIT(ASSET_CONFIG, 28, bnnn_paging, step_asset_config_metadata_hash(),  {"Metadata hash",  text});
ALGO_UX_STEP_NOCB_INIT(ASSET_CONFIG, 29, bnnn_paging, step_asset_config_manager(),        {"Manager",        text});
ALGO_UX_STEP_NOCB_INIT(ASSET_CONFIG, 30, bnnn_paging, step_asset_config_reserve(),        {"Reserve",        text});
ALGO_UX_STEP_NOCB_INIT(ASSET_CONFIG, 31, bnnn_paging, step_asset_config_freeze(),         {"Freezer",        text});
ALGO_UX_STEP_NOCB_INIT(ASSET_CONFIG, 32, bnnn_paging, step_asset_config_clawback(),       {"Clawback",       text});

ALGO_UX_STEP(33, pbb, NULL, 0, txn_approve(), NULL, {&C_icon_validate_14, "Sign",   "transaction"});
ALGO_UX_STEP(34, pbb, NULL, 0, user_approval_denied(),    NULL, {&C_icon_crossmark,   "Cancel", "signature"});

UX_FLOW(ux_txn_flow,
  &txn_flow_0,
  &txn_flow_1,
  &txn_flow_2,
  // &txn_flow_3,
  // &txn_flow_4,
  &txn_flow_5,
  &txn_flow_6,
  &txn_flow_7,
  &txn_flow_8,
  &txn_flow_9,
  &txn_flow_10,
  &txn_flow_11,
  &txn_flow_12,
  &txn_flow_13,
  &txn_flow_14,
  &txn_flow_15,
  &txn_flow_16,
  &txn_flow_17,
  &txn_flow_18,
  &txn_flow_19,
  &txn_flow_20,
  &txn_flow_21,
  &txn_flow_22,
  &txn_flow_23,
  &txn_flow_24,
  &txn_flow_25,
  &txn_flow_26,
  &txn_flow_27,
  &txn_flow_28,
  &txn_flow_29,
  &txn_flow_30,
  &txn_flow_31,
  &txn_flow_32
);

UX_FLOW(ux_approve_txn_flow,
  &txn_flow_33,
  &txn_flow_34
);

void ui_txn()
{
  PRINTF("Transaction:\n");
  PRINTF("  Type: %d\n", current_txn.type);
  PRINTF("  Sender: %.*h\n", 32, current_txn.sender);
  PRINTF("  Fee: %s\n", u64str(current_txn.fee));
  PRINTF("  First valid: %s\n", u64str(current_txn.firstValid));
  PRINTF("  Last valid: %s\n", u64str(current_txn.lastValid));
  PRINTF("  Genesis ID: %.*s\n", 32, current_txn.genesisID);
  PRINTF("  Genesis hash: %.*h\n", 32, current_txn.genesisHash);
  if (current_txn.type == PAYMENT) {
    PRINTF("  Receiver: %.*h\n", 32, current_txn.payment.receiver);
    PRINTF("  Amount: %s\n", u64str(current_txn.payment.amount));
    PRINTF("  Close to: %.*h\n", 32, current_txn.payment.close);
  }
  if (current_txn.type == KEYREG) {
    PRINTF("  Vote PK: %.*h\n", 32, current_txn.keyreg.votepk);
    PRINTF("  VRF PK: %.*h\n", 32, current_txn.keyreg.vrfpk);
  }

  ux_last_step = 0;
  if (G_ux.stack_count == 0) {
    ux_stack_push();
  }
  ux_flow_init(0, ux_txn_flow, NULL);
}

void ux_approve_txn()
{
  if (G_ux.stack_count == 0) {
    ux_stack_push();
  }
  ux_flow_init(0, ux_approve_txn_flow, NULL);
}
