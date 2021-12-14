// SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause
/*
 * Copyright (C) 2012-2014, 2018-2021 Intel Corporation
 * Copyright (C) 2013-2015 Intel Mobile Communications GmbH
 * Copyright (C) 2016-2017 Intel Deutschland GmbH
 */
#include <linux/etherdevice.h>
#include <net/netlink.h>
#include <net/mac80211.h>
#include "mvm.h"
#include "iwl-vendor-cmd.h"
#include "fw/api/datapath.h"

#include "iwl-io.h"
#include "iwl-prph.h"

static LIST_HEAD(device_list);
static DEFINE_SPINLOCK(device_list_lock);

static int iwl_mvm_netlink_notifier(struct notifier_block *nb,
				    unsigned long state,
				    void *_notify)
{
	struct netlink_notify *notify = _notify;
	struct iwl_mvm *mvm;

	if (state != NETLINK_URELEASE || notify->protocol != NETLINK_GENERIC)
		return NOTIFY_DONE;

	spin_lock_bh(&device_list_lock);
	list_for_each_entry(mvm, &device_list, list) {
		if (mvm->csi_portid == netlink_notify_portid(notify))
			mvm->csi_portid = 0;
	}
	spin_unlock_bh(&device_list_lock);

	return NOTIFY_OK;
}

static struct notifier_block iwl_mvm_netlink_notifier_block = {
	.notifier_call = iwl_mvm_netlink_notifier,
};

void iwl_mvm_vendor_cmd_init(void)
{
	WARN_ON(netlink_register_notifier(&iwl_mvm_netlink_notifier_block));
	spin_lock_init(&device_list_lock);
}

void iwl_mvm_vendor_cmd_exit(void)
{
	netlink_unregister_notifier(&iwl_mvm_netlink_notifier_block);
}

static const struct nla_policy
iwl_mvm_vendor_attr_policy[NUM_IWL_MVM_VENDOR_ATTR] = {
	[IWL_MVM_VENDOR_ATTR_LOW_LATENCY] = { .type = NLA_FLAG },
	[IWL_MVM_VENDOR_ATTR_COUNTRY] = { .type = NLA_STRING, .len = 2 },
	[IWL_MVM_VENDOR_ATTR_FILTER_ARP_NA] = { .type = NLA_FLAG },
	[IWL_MVM_VENDOR_ATTR_FILTER_GTK] = { .type = NLA_FLAG },
	[IWL_MVM_VENDOR_ATTR_ADDR] = { .type = NLA_BINARY, .len = ETH_ALEN },
	[IWL_MVM_VENDOR_ATTR_TXP_LIMIT_24] = { .type = NLA_U32 },
	[IWL_MVM_VENDOR_ATTR_TXP_LIMIT_52L] = { .type = NLA_U32 },
	[IWL_MVM_VENDOR_ATTR_TXP_LIMIT_52H] = { .type = NLA_U32 },
	[IWL_MVM_VENDOR_ATTR_OPPPS_WA] = { .type = NLA_FLAG },
	[IWL_MVM_VENDOR_ATTR_GSCAN_MAC_ADDR] = { .len = ETH_ALEN },
	[IWL_MVM_VENDOR_ATTR_GSCAN_MAC_ADDR_MASK] = { .len = ETH_ALEN },
	[IWL_MVM_VENDOR_ATTR_GSCAN_MAX_AP_PER_SCAN] = { .type = NLA_U32 },
	[IWL_MVM_VENDOR_ATTR_GSCAN_REPORT_THRESHOLD] = { .type = NLA_U32 },
	[IWL_MVM_VENDOR_ATTR_GSCAN_BUCKET_SPECS] = { .type = NLA_NESTED },
	[IWL_MVM_VENDOR_ATTR_GSCAN_LOST_AP_SAMPLE_SIZE] = { .type = NLA_U8 },
	[IWL_MVM_VENDOR_ATTR_GSCAN_AP_LIST] = { .type = NLA_NESTED },
	[IWL_MVM_VENDOR_ATTR_GSCAN_RSSI_SAMPLE_SIZE] = { .type = NLA_U8 },
	[IWL_MVM_VENDOR_ATTR_GSCAN_MIN_BREACHING] = { .type = NLA_U8 },
	[IWL_MVM_VENDOR_ATTR_RXFILTER] = { .type = NLA_U32 },
	[IWL_MVM_VENDOR_ATTR_RXFILTER_OP] = { .type = NLA_U32 },
	[IWL_MVM_VENDOR_ATTR_DBG_COLLECT_TRIGGER] = { .type = NLA_STRING },
	[IWL_MVM_VENDOR_ATTR_NAN_FAW_FREQ] = { .type = NLA_U32 },
	[IWL_MVM_VENDOR_ATTR_NAN_FAW_SLOTS] = { .type = NLA_U8 },
	[IWL_MVM_VENDOR_ATTR_GSCAN_REPORT_THRESHOLD_NUM] = { .type = NLA_U32 },
	[IWL_MVM_VENDOR_ATTR_SAR_CHAIN_A_PROFILE] = { .type = NLA_U8 },
	[IWL_MVM_VENDOR_ATTR_SAR_CHAIN_B_PROFILE] = { .type = NLA_U8 },
	[IWL_MVM_VENDOR_ATTR_FIPS_TEST_VECTOR_HW_CCM] = { .type = NLA_NESTED },
	[IWL_MVM_VENDOR_ATTR_FIPS_TEST_VECTOR_HW_GCM] = { .type = NLA_NESTED },
	[IWL_MVM_VENDOR_ATTR_FIPS_TEST_VECTOR_HW_AES] = { .type = NLA_NESTED },
	[IWL_MVM_VENDOR_ATTR_STA_CIPHER] = { .type = NLA_U32 },
	[IWL_MVM_VENDOR_ATTR_STA_HLTK] = { .type = NLA_BINARY },
	[IWL_MVM_VENDOR_ATTR_STA_TK] = { .type = NLA_BINARY },
	[IWL_MVM_VENDOR_ATTR_RFIM_INFO]		    = { .type = NLA_NESTED },
	[IWL_MVM_VENDOR_ATTR_RFIM_FREQ]		    = { .type = NLA_U32 },
	[IWL_MVM_VENDOR_ATTR_RFIM_CHANNELS]	    = { .type = NLA_U32 },
	[IWL_MVM_VENDOR_ATTR_RFIM_BANDS]	    = { .type = NLA_U32 },
	[IWL_MVM_VENDOR_ATTR_TIME_SYNC_PROTOCOL_TYPE] = { .type = NLA_U32 },
	[IWL_MVM_VENDOR_ATTR_TIME_SYNC_DIALOG_TOKEN] = { .type = NLA_U32 },
	[IWL_MVM_VENDOR_ATTR_TIME_SYNC_T1] = { .type = NLA_U64 },
	[IWL_MVM_VENDOR_ATTR_TIME_SYNC_T1_MAX_ERROR] = { .type = NLA_U32 },
	[IWL_MVM_VENDOR_ATTR_TIME_SYNC_T4] = { .type = NLA_U64 },
	[IWL_MVM_VENDOR_ATTR_TIME_SYNC_T4_MAX_ERROR] = { .type = NLA_U32 },
	[IWL_MVM_VENDOR_ATTR_TIME_SYNC_FUP_DIALOG_TOKEN] = { .type = NLA_U32 },
	[IWL_MVM_VENDOR_ATTR_TIME_SYNC_T2] = { .type = NLA_U64 },
	[IWL_MVM_VENDOR_ATTR_TIME_SYNC_T2_MAX_ERROR] = { .type = NLA_U32 },
	[IWL_MVM_VENDOR_ATTR_TIME_SYNC_T3] = { .type = NLA_U64 },
	[IWL_MVM_VENDOR_ATTR_TIME_SYNC_T3_MAX_ERROR] = { .type = NLA_U32 },
	[IWL_MVM_VENDOR_ATTR_ROAMING_FORBIDDEN] = { .type = NLA_U8 },
	[IWL_MVM_VENDOR_ATTR_AUTH_MODE] = { .type = NLA_U32 },
	[IWL_MVM_VENDOR_ATTR_CHANNEL_NUM] = { .type = NLA_U8 },
	[IWL_MVM_VENDOR_ATTR_SSID] = { .type = NLA_BINARY,
				       .len = IEEE80211_MAX_SSID_LEN },
	[IWL_MVM_VENDOR_ATTR_BAND] = { .type = NLA_U8 },
	[IWL_MVM_VENDOR_ATTR_COLLOC_CHANNEL] = { .type = NLA_U8 },
	[IWL_MVM_VENDOR_ATTR_COLLOC_ADDR] = { .type = NLA_BINARY, .len = ETH_ALEN },
};

static struct nlattr **iwl_mvm_parse_vendor_data(const void *data, int data_len)
{
	struct nlattr **tb;
	int err;

	if (!data)
		return ERR_PTR(-EINVAL);

	tb = kcalloc(MAX_IWL_MVM_VENDOR_ATTR + 1, sizeof(*tb), GFP_KERNEL);
	if (!tb)
		return ERR_PTR(-ENOMEM);

	err = nla_parse(tb, MAX_IWL_MVM_VENDOR_ATTR, data, data_len,
			iwl_mvm_vendor_attr_policy, NULL);
	if (err) {
		kfree(tb);
		return ERR_PTR(err);
	}

	return tb;
}

static int iwl_mvm_set_low_latency(struct wiphy *wiphy,
				   struct wireless_dev *wdev,
				   const void *data, int data_len)
{
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct iwl_mvm *mvm = IWL_MAC80211_GET_MVM(hw);
	struct nlattr **tb;
	int err;
	struct ieee80211_vif *vif = wdev_to_ieee80211_vif(wdev);
	bool low_latency;

	if (!vif)
		return -ENODEV;

	if (data) {
		tb = iwl_mvm_parse_vendor_data(data, data_len);
		if (IS_ERR(tb))
			return PTR_ERR(tb);
		low_latency = tb[IWL_MVM_VENDOR_ATTR_LOW_LATENCY];
		kfree(tb);
	} else {
		low_latency = false;
	}

	mutex_lock(&mvm->mutex);
	err = iwl_mvm_update_low_latency(mvm, vif, low_latency,
					 LOW_LATENCY_VCMD);
	mutex_unlock(&mvm->mutex);

	return err;
}

static int iwl_mvm_get_low_latency(struct wiphy *wiphy,
				   struct wireless_dev *wdev,
				   const void *data, int data_len)
{
	struct ieee80211_vif *vif = wdev_to_ieee80211_vif(wdev);
	struct iwl_mvm_vif *mvmvif;
	struct sk_buff *skb;

	if (!vif)
		return -ENODEV;
	mvmvif = iwl_mvm_vif_from_mac80211(vif);

	skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, 100);
	if (!skb)
		return -ENOMEM;
	if (iwl_mvm_vif_low_latency(mvmvif) &&
	    nla_put_flag(skb, IWL_MVM_VENDOR_ATTR_LOW_LATENCY)) {
		kfree_skb(skb);
		return -ENOBUFS;
	}

	return cfg80211_vendor_cmd_reply(skb);
}

static int iwl_mvm_set_country(struct wiphy *wiphy,
			       struct wireless_dev *wdev,
			       const void *data, int data_len)
{
	struct ieee80211_regdomain *regd;
	struct nlattr **tb;
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct iwl_mvm *mvm = IWL_MAC80211_GET_MVM(hw);
	int retval;

	if (!iwl_mvm_is_lar_supported(mvm))
		return -EOPNOTSUPP;

	tb = iwl_mvm_parse_vendor_data(data, data_len);
	if (IS_ERR(tb))
		return PTR_ERR(tb);

	if (!tb[IWL_MVM_VENDOR_ATTR_COUNTRY]) {
		retval = -EINVAL;
		goto free;
	}

	mutex_lock(&mvm->mutex);

	/* set regdomain information to FW */
	regd = iwl_mvm_get_regdomain(wiphy,
				     nla_data(tb[IWL_MVM_VENDOR_ATTR_COUNTRY]),
				     iwl_mvm_is_wifi_mcc_supported(mvm) ?
				     MCC_SOURCE_3G_LTE_HOST :
				     MCC_SOURCE_OLD_FW, NULL);
	if (IS_ERR_OR_NULL(regd)) {
		retval = -EIO;
		goto unlock;
	}

	retval = regulatory_set_wiphy_regd(wiphy, regd);
	kfree(regd);
unlock:
	mutex_unlock(&mvm->mutex);
free:
	kfree(tb);
	return retval;
}

#ifdef CPTCFG_IWLMVM_TDLS_PEER_CACHE
static int iwl_vendor_tdls_peer_cache_add(struct wiphy *wiphy,
					  struct wireless_dev *wdev,
					  const void *data, int data_len)
{
	struct nlattr **tb;
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct iwl_mvm *mvm = IWL_MAC80211_GET_MVM(hw);
	struct iwl_mvm_tdls_peer_counter *cnt;
	u8 *addr;
	struct ieee80211_vif *vif = wdev_to_ieee80211_vif(wdev);
	int err;

	if (!vif)
		return -ENODEV;

	tb = iwl_mvm_parse_vendor_data(data, data_len);
	if (IS_ERR(tb))
		return PTR_ERR(tb);

	if (vif->type != NL80211_IFTYPE_STATION ||
	    !tb[IWL_MVM_VENDOR_ATTR_ADDR]) {
		err = -EINVAL;
		goto free;
	}

	mutex_lock(&mvm->mutex);
	if (mvm->tdls_peer_cache_cnt >= IWL_MVM_TDLS_CNT_MAX_PEERS) {
		err = -ENOSPC;
		goto out_unlock;
	}

	addr = nla_data(tb[IWL_MVM_VENDOR_ATTR_ADDR]);

	rcu_read_lock();
	cnt = iwl_mvm_tdls_peer_cache_find(mvm, addr);
	rcu_read_unlock();
	if (cnt) {
		err = -EEXIST;
		goto out_unlock;
	}

	cnt = kzalloc(sizeof(*cnt) +
		      sizeof(cnt->rx[0]) * mvm->trans->num_rx_queues,
		      GFP_KERNEL);
	if (!cnt) {
		err = -ENOMEM;
		goto out_unlock;
	}

	IWL_DEBUG_TDLS(mvm, "Adding %pM to TDLS peer cache\n", addr);
	ether_addr_copy(cnt->mac.addr, addr);
	cnt->vif = vif;
	list_add_tail_rcu(&cnt->list, &mvm->tdls_peer_cache_list);
	mvm->tdls_peer_cache_cnt++;

	err = 0;

out_unlock:
	mutex_unlock(&mvm->mutex);
free:
	kfree(tb);
	return err;
}

static int iwl_vendor_tdls_peer_cache_del(struct wiphy *wiphy,
					  struct wireless_dev *wdev,
					  const void *data, int data_len)
{
	struct nlattr **tb;
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct iwl_mvm *mvm = IWL_MAC80211_GET_MVM(hw);
	struct iwl_mvm_tdls_peer_counter *cnt;
	u8 *addr;
	int err;

	tb = iwl_mvm_parse_vendor_data(data, data_len);
	if (IS_ERR(tb))
		return PTR_ERR(tb);

	if (!tb[IWL_MVM_VENDOR_ATTR_ADDR]) {
		err = -EINVAL;
		goto free;
	}

	addr = nla_data(tb[IWL_MVM_VENDOR_ATTR_ADDR]);

	mutex_lock(&mvm->mutex);
	rcu_read_lock();
	cnt = iwl_mvm_tdls_peer_cache_find(mvm, addr);
	if (!cnt) {
		IWL_DEBUG_TDLS(mvm, "%pM not found in TDLS peer cache\n", addr);
		err = -ENOENT;
		goto out_unlock;
	}

	IWL_DEBUG_TDLS(mvm, "Removing %pM from TDLS peer cache\n", addr);
	mvm->tdls_peer_cache_cnt--;
	list_del_rcu(&cnt->list);
	kfree_rcu(cnt, rcu_head);
	err = 0;

out_unlock:
	rcu_read_unlock();
	mutex_unlock(&mvm->mutex);
free:
	kfree(tb);
	return err;
}

static int iwl_vendor_tdls_peer_cache_query(struct wiphy *wiphy,
					    struct wireless_dev *wdev,
					    const void *data, int data_len)
{
	struct nlattr **tb;
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct iwl_mvm *mvm = IWL_MAC80211_GET_MVM(hw);
	struct iwl_mvm_tdls_peer_counter *cnt;
	struct sk_buff *skb;
	u32 rx_bytes, tx_bytes;
	u8 *addr;
	int err;

	tb = iwl_mvm_parse_vendor_data(data, data_len);
	if (IS_ERR(tb))
		return PTR_ERR(tb);

	if (!tb[IWL_MVM_VENDOR_ATTR_ADDR]) {
		kfree(tb);
		return -EINVAL;
	}

	addr = nla_data(tb[IWL_MVM_VENDOR_ATTR_ADDR]);

	/* we can free the tb, the addr pointer is still valid into the msg */
	kfree(tb);

	rcu_read_lock();
	cnt = iwl_mvm_tdls_peer_cache_find(mvm, addr);
	if (!cnt) {
		IWL_DEBUG_TDLS(mvm, "%pM not found in TDLS peer cache\n",
			       addr);
		err = -ENOENT;
	} else {
		int q;

		tx_bytes = cnt->tx_bytes;
		rx_bytes = 0;
		for (q = 0; q < mvm->trans->num_rx_queues; q++)
			rx_bytes += cnt->rx[q].bytes;
		err = 0;
	}
	rcu_read_unlock();
	if (err)
		return err;

	skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, 100);
	if (!skb)
		return -ENOMEM;
	if (nla_put_u32(skb, IWL_MVM_VENDOR_ATTR_TX_BYTES, tx_bytes) ||
	    nla_put_u32(skb, IWL_MVM_VENDOR_ATTR_RX_BYTES, rx_bytes)) {
		kfree_skb(skb);
		return -ENOBUFS;
	}

	return cfg80211_vendor_cmd_reply(skb);
}
#endif /* CPTCFG_IWLMVM_TDLS_PEER_CACHE */

#define IWL_MVM_RFIM_CAPA_CNVI  (BIT(2))
#define IWL_MVM_RFIM_CAPA_SCAN  (BIT(3))
#define IWL_MVM_RFIM_CAPA_ASSOC (BIT(4))
#define IWL_MVM_RFIM_CAPA_TPT   (BIT(5))
#define IWL_MVM_RFIM_CAPA_ALL	(IWL_MVM_RFIM_CAPA_CNVI	  |\
				IWL_MVM_RFIM_CAPA_SCAN    |\
				IWL_MVM_RFIM_CAPA_ASSOC   |\
				IWL_MVM_RFIM_CAPA_TPT)

static int iwl_vendor_rfim_get_capa(struct wiphy *wiphy,
				    struct wireless_dev *wdev,
				    const void *data, int data_len)
{
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct iwl_mvm *mvm = IWL_MAC80211_GET_MVM(hw);
	struct sk_buff *skb;
	u8 capa = 0;

	skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, 4);
	if (!skb)
		return -ENOMEM;

	if (mvm->trans->trans_cfg->device_family >= IWL_DEVICE_FAMILY_AX210 &&
	    mvm->trans->trans_cfg->integrated &&
	    fw_has_capa(&mvm->fw->ucode_capa, IWL_UCODE_TLV_CAPA_RFIM_SUPPORT))
		capa = IWL_MVM_RFIM_CAPA_ALL;

	if (nla_put_u8(skb, IWL_MVM_VENDOR_ATTR_RFIM_CAPA, capa)) {
		kfree_skb(skb);
		return -ENOBUFS;
	}

	return cfg80211_vendor_cmd_reply(skb);
}

static int iwl_vendor_rfim_get_table(struct wiphy *wiphy,
				     struct wireless_dev *wdev,
				     const void *data, int data_len)
{
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct iwl_mvm *mvm = IWL_MAC80211_GET_MVM(hw);
	struct iwl_rfi_freq_table_resp_cmd *resp;
	struct sk_buff *skb = NULL;
	struct nlattr *rfim_info;
	int i, ret;

	resp = iwl_rfi_get_freq_table(mvm);

	if (IS_ERR(resp))
		return PTR_ERR(resp);

	if (resp->status != RFI_FREQ_TABLE_OK) {
		ret = -EINVAL;
		goto err;
	}

	skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, sizeof(*rfim_info) + 100);
	if (!skb) {
		ret = -ENOMEM;
		goto err;
	}

	rfim_info = nla_nest_start(skb, IWL_MVM_VENDOR_ATTR_RFIM_INFO |
					NLA_F_NESTED);
	if (!rfim_info) {
		ret = -ENOBUFS;
		goto err;
	}

	for (i = 0; i < 4; i++) {
		if (nla_put_u16(skb, IWL_MVM_VENDOR_ATTR_RFIM_FREQ,
				le16_to_cpu(resp->table[i].freq)) ||
		    nla_put(skb, IWL_MVM_VENDOR_ATTR_RFIM_CHANNELS,
			    sizeof(resp->table[i].channels),
			    resp->table[i].channels) ||
		    nla_put(skb, IWL_MVM_VENDOR_ATTR_RFIM_BANDS,
			    sizeof(resp->table[i].bands),
			    resp->table[i].bands)) {
			ret = -ENOBUFS;
			goto err;
		}
	}

	nla_nest_end(skb, rfim_info);

	kfree(resp);
	return cfg80211_vendor_cmd_reply(skb);

err:
	kfree_skb(skb);
	kfree(resp);
	return ret;
}

static int iwl_vendor_rfim_set_table(struct wiphy *wiphy,
				     struct wireless_dev *wdev,
				     const void *data, int data_len)
{
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct iwl_mvm *mvm = IWL_MAC80211_GET_MVM(hw);
	struct iwl_rfi_lut_entry rfim_table[IWL_RFI_LUT_SIZE] = {};
	struct nlattr **tb;
	struct nlattr *attr;
	int rem, err = 0;
	int row_idx = -1; /* the row is updated only at frequency attr */

	tb = iwl_mvm_parse_vendor_data(data, data_len);
	if (IS_ERR(tb))
		return PTR_ERR(tb);

	if (!tb[IWL_MVM_VENDOR_ATTR_RFIM_INFO]) {
		err = -EINVAL;
		goto out;
	}

	nla_for_each_nested(attr, tb[IWL_MVM_VENDOR_ATTR_RFIM_INFO], rem) {
		switch (nla_type(attr)) {
		case IWL_MVM_VENDOR_ATTR_RFIM_FREQ:
			row_idx++;
			rfim_table[row_idx].freq =
				cpu_to_le16(nla_get_u16(attr));
			break;
		case IWL_MVM_VENDOR_ATTR_RFIM_CHANNELS:
			if (row_idx < 0) {
				err = -EINVAL;
				goto out;
			}
			memcpy(rfim_table[row_idx].channels, nla_data(attr),
			       ARRAY_SIZE(rfim_table[row_idx].channels));
			break;
		case IWL_MVM_VENDOR_ATTR_RFIM_BANDS:
			if (row_idx < 0) {
				err = -EINVAL;
				goto out;
			}
			memcpy(rfim_table[row_idx].bands, nla_data(attr),
			       ARRAY_SIZE(rfim_table[row_idx].bands));
			break;
		default:
			IWL_ERR(mvm, "Invalid attribute %d\n", nla_type(attr));
			err = -EINVAL;
			goto out;
		}
	}

	err = iwl_rfi_send_config_cmd(mvm, rfim_table);
	if (err)
		IWL_ERR(mvm, "Failed to send rfi table to FW, error %d\n", err);

out:
	kfree(tb);
	return err;
}

static int iwl_vendor_set_nic_txpower_limit(struct wiphy *wiphy,
					    struct wireless_dev *wdev,
					    const void *data, int data_len)
{
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct iwl_mvm *mvm = IWL_MAC80211_GET_MVM(hw);
	struct iwl_dev_tx_power_cmd cmd = {
		.common.set_mode = cpu_to_le32(IWL_TX_POWER_MODE_SET_DEVICE),
		.common.dev_24 = cpu_to_le16(IWL_DEV_MAX_TX_POWER),
		.common.dev_52_low = cpu_to_le16(IWL_DEV_MAX_TX_POWER),
		.common.dev_52_high = cpu_to_le16(IWL_DEV_MAX_TX_POWER),
	};
	struct nlattr **tb;
	int len;
	int err;
	u8 cmd_ver = iwl_fw_lookup_cmd_ver(mvm->fw, REDUCE_TX_POWER_CMD,
					   IWL_FW_CMD_VER_UNKNOWN);

	tb = iwl_mvm_parse_vendor_data(data, data_len);
	if (IS_ERR(tb))
		return PTR_ERR(tb);

	if (tb[IWL_MVM_VENDOR_ATTR_TXP_LIMIT_24]) {
		s32 txp = nla_get_u32(tb[IWL_MVM_VENDOR_ATTR_TXP_LIMIT_24]);

		if (txp < 0 || txp > IWL_DEV_MAX_TX_POWER) {
			err = -EINVAL;
			goto free;
		}
		cmd.common.dev_24 = cpu_to_le16(txp);
	}

	if (tb[IWL_MVM_VENDOR_ATTR_TXP_LIMIT_52L]) {
		s32 txp = nla_get_u32(tb[IWL_MVM_VENDOR_ATTR_TXP_LIMIT_52L]);

		if (txp < 0 || txp > IWL_DEV_MAX_TX_POWER) {
			err = -EINVAL;
			goto free;
		}
		cmd.common.dev_52_low = cpu_to_le16(txp);
	}

	if (tb[IWL_MVM_VENDOR_ATTR_TXP_LIMIT_52H]) {
		s32 txp = nla_get_u32(tb[IWL_MVM_VENDOR_ATTR_TXP_LIMIT_52H]);

		if (txp < 0 || txp > IWL_DEV_MAX_TX_POWER) {
			err = -EINVAL;
			goto free;
		}
		cmd.common.dev_52_high = cpu_to_le16(txp);
	}

	if (cmd_ver == 6)
		len = sizeof(mvm->txp_cmd.v6);
	else if (fw_has_api(&mvm->fw->ucode_capa,
			    IWL_UCODE_TLV_API_REDUCE_TX_POWER))
		len = sizeof(mvm->txp_cmd.v5);
	else if (fw_has_capa(&mvm->fw->ucode_capa,
			     IWL_UCODE_TLV_CAPA_TX_POWER_ACK))
		len = sizeof(mvm->txp_cmd.v4);
	else
		len = sizeof(mvm->txp_cmd.v3);

	/* all structs have the same common part, add it */
	len += sizeof(cmd.common);

	mutex_lock(&mvm->mutex);
	if (iwl_mvm_firmware_running(mvm))
		err = iwl_mvm_send_cmd_pdu(mvm, REDUCE_TX_POWER_CMD,
					   0, len, &cmd);
	else
		err = 0;

	if (err)
		IWL_ERR(mvm, "failed to update device TX power: %d\n", err);
	else
		mvm->txp_cmd = cmd;
	mutex_unlock(&mvm->mutex);
	err = 0;
free:
	kfree(tb);
	return err;
}

#ifdef CPTCFG_IWLMVM_P2P_OPPPS_TEST_WA
static int iwl_mvm_oppps_wa_update_quota(struct iwl_mvm *mvm,
					 struct ieee80211_vif *vif,
					 bool enable)
{
	struct iwl_mvm_vif *mvmvif = iwl_mvm_vif_from_mac80211(vif);
	struct ieee80211_p2p_noa_attr *noa = &vif->bss_conf.p2p_noa_attr;
	bool force_update = true;

	if (enable && noa->oppps_ctwindow & IEEE80211_P2P_OPPPS_ENABLE_BIT)
		mvm->p2p_opps_test_wa_vif = mvmvif;
	else
		mvm->p2p_opps_test_wa_vif = NULL;

	if (fw_has_capa(&mvm->fw->ucode_capa,
			IWL_UCODE_TLV_CAPA_DYNAMIC_QUOTA)) {
#ifdef CPTCFG_IWLWIFI_DHC_PRIVATE
		return iwl_mvm_dhc_quota_enforce(mvm,
						 mvm->p2p_opps_test_wa_vif,
						 0);
#else
		return -EOPNOTSUPP;
#endif
	}

	return iwl_mvm_update_quotas(mvm, force_update, NULL);
}

static int iwl_mvm_oppps_wa(struct wiphy *wiphy,
			    struct wireless_dev *wdev,
			    const void *data, int data_len)
{
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct iwl_mvm *mvm = IWL_MAC80211_GET_MVM(hw);
	struct nlattr **tb;
	int err;
	struct ieee80211_vif *vif = wdev_to_ieee80211_vif(wdev);

	if (!vif)
		return -ENODEV;

	tb = iwl_mvm_parse_vendor_data(data, data_len);
	if (IS_ERR(tb))
		return PTR_ERR(tb);

	mutex_lock(&mvm->mutex);
	if (vif->type == NL80211_IFTYPE_STATION && vif->p2p) {
		bool enable = !!tb[IWL_MVM_VENDOR_ATTR_OPPPS_WA];

		err = iwl_mvm_oppps_wa_update_quota(mvm, vif, enable);
	} else {
		err = -EOPNOTSUPP;
	}
	mutex_unlock(&mvm->mutex);

	kfree(tb);
	return err;
}
#endif

void iwl_mvm_active_rx_filters(struct iwl_mvm *mvm)
{
	int i, len, total = 0;
	struct iwl_mcast_filter_cmd *cmd;
	static const u8 ipv4mc[] = {0x01, 0x00, 0x5e};
	static const u8 ipv6mc[] = {0x33, 0x33};
	static const u8 ipv4_mdns[] = {0x01, 0x00, 0x5e, 0x00, 0x00, 0xfb};
	static const u8 ipv6_mdns[] = {0x33, 0x33, 0x00, 0x00, 0x00, 0xfb};

	lockdep_assert_held(&mvm->mutex);

	if (mvm->rx_filters & IWL_MVM_VENDOR_RXFILTER_EINVAL)
		return;

	for (i = 0; i < mvm->mcast_filter_cmd->count; i++) {
		if (mvm->rx_filters & IWL_MVM_VENDOR_RXFILTER_MCAST4 &&
		    memcmp(&mvm->mcast_filter_cmd->addr_list[i * ETH_ALEN],
			   ipv4mc, sizeof(ipv4mc)) == 0)
			total++;
		else if (memcmp(&mvm->mcast_filter_cmd->addr_list[i * ETH_ALEN],
				ipv4_mdns, sizeof(ipv4_mdns)) == 0)
			total++;
		else if (mvm->rx_filters & IWL_MVM_VENDOR_RXFILTER_MCAST6 &&
			 memcmp(&mvm->mcast_filter_cmd->addr_list[i * ETH_ALEN],
				ipv6mc, sizeof(ipv6mc)) == 0)
			total++;
		else if (memcmp(&mvm->mcast_filter_cmd->addr_list[i * ETH_ALEN],
				ipv6_mdns, sizeof(ipv6_mdns)) == 0)
			total++;
	}

	/* FW expects full words */
	len = roundup(sizeof(*cmd) + total * ETH_ALEN, 4);
	cmd = kzalloc(len, GFP_KERNEL);
	if (!cmd)
		return;

	memcpy(cmd, mvm->mcast_filter_cmd, sizeof(*cmd));
	cmd->count = 0;

	for (i = 0; i < mvm->mcast_filter_cmd->count; i++) {
		bool copy_filter = false;

		if (mvm->rx_filters & IWL_MVM_VENDOR_RXFILTER_MCAST4 &&
		    memcmp(&mvm->mcast_filter_cmd->addr_list[i * ETH_ALEN],
			   ipv4mc, sizeof(ipv4mc)) == 0)
			copy_filter = true;
		else if (memcmp(&mvm->mcast_filter_cmd->addr_list[i * ETH_ALEN],
				ipv4_mdns, sizeof(ipv4_mdns)) == 0)
			copy_filter = true;
		else if (mvm->rx_filters & IWL_MVM_VENDOR_RXFILTER_MCAST6 &&
			 memcmp(&mvm->mcast_filter_cmd->addr_list[i * ETH_ALEN],
				ipv6mc, sizeof(ipv6mc)) == 0)
			copy_filter = true;
		else if (memcmp(&mvm->mcast_filter_cmd->addr_list[i * ETH_ALEN],
				ipv6_mdns, sizeof(ipv6_mdns)) == 0)
			copy_filter = true;

		if (!copy_filter)
			continue;

		ether_addr_copy(&cmd->addr_list[cmd->count * ETH_ALEN],
				&mvm->mcast_filter_cmd->addr_list[i * ETH_ALEN]);
		cmd->count++;
	}

	kfree(mvm->mcast_active_filter_cmd);
	mvm->mcast_active_filter_cmd = cmd;
}

static int iwl_mvm_vendor_rxfilter(struct wiphy *wiphy,
				   struct wireless_dev *wdev,
				   const void *data, int data_len)
{
	struct nlattr **tb;
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct iwl_mvm *mvm = IWL_MAC80211_GET_MVM(hw);
	enum iwl_mvm_vendor_rxfilter_flags filter, rx_filters, old_rx_filters;
	enum iwl_mvm_vendor_rxfilter_op op;
	bool first_set;
	u32 mask;
	int err;

	tb = iwl_mvm_parse_vendor_data(data, data_len);
	if (IS_ERR(tb))
		return PTR_ERR(tb);

	if (!tb[IWL_MVM_VENDOR_ATTR_RXFILTER]) {
		err = -EINVAL;
		goto free;
	}

	if (!tb[IWL_MVM_VENDOR_ATTR_RXFILTER_OP]) {
		err = -EINVAL;
		goto free;
	}

	filter = nla_get_u32(tb[IWL_MVM_VENDOR_ATTR_RXFILTER]);
	op = nla_get_u32(tb[IWL_MVM_VENDOR_ATTR_RXFILTER_OP]);

	if (filter != IWL_MVM_VENDOR_RXFILTER_UNICAST &&
	    filter != IWL_MVM_VENDOR_RXFILTER_BCAST &&
	    filter != IWL_MVM_VENDOR_RXFILTER_MCAST4 &&
	    filter != IWL_MVM_VENDOR_RXFILTER_MCAST6) {
		err = -EINVAL;
		goto free;
	}

	rx_filters = mvm->rx_filters & ~IWL_MVM_VENDOR_RXFILTER_EINVAL;
	switch (op) {
	case IWL_MVM_VENDOR_RXFILTER_OP_DROP:
		rx_filters &= ~filter;
		break;
	case IWL_MVM_VENDOR_RXFILTER_OP_PASS:
		rx_filters |= filter;
		break;
	default:
		err = -EINVAL;
		goto free;
	}

	first_set = mvm->rx_filters & IWL_MVM_VENDOR_RXFILTER_EINVAL;

	/* If first time set - clear EINVAL value */
	mvm->rx_filters &= ~IWL_MVM_VENDOR_RXFILTER_EINVAL;

	err = 0;

	if (rx_filters == mvm->rx_filters && !first_set)
		goto free;

	mutex_lock(&mvm->mutex);

	old_rx_filters = mvm->rx_filters;
	mvm->rx_filters = rx_filters;

	mask = IWL_MVM_VENDOR_RXFILTER_MCAST4 | IWL_MVM_VENDOR_RXFILTER_MCAST6;
	if ((old_rx_filters & mask) != (rx_filters & mask) || first_set) {
		iwl_mvm_active_rx_filters(mvm);
		iwl_mvm_recalc_multicast(mvm);
	}

	mask = IWL_MVM_VENDOR_RXFILTER_BCAST;
	if ((old_rx_filters & mask) != (rx_filters & mask) || first_set)
		iwl_mvm_configure_bcast_filter(mvm);

	mutex_unlock(&mvm->mutex);

free:
	kfree(tb);
	return err;
}

static int iwl_mvm_vendor_dbg_collect(struct wiphy *wiphy,
				      struct wireless_dev *wdev,
				      const void *data, int data_len)
{
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct iwl_mvm *mvm = IWL_MAC80211_GET_MVM(hw);
	struct nlattr **tb;
	int err, len = 0;
	const char *trigger_desc;

	tb = iwl_mvm_parse_vendor_data(data, data_len);
	if (IS_ERR(tb))
		return PTR_ERR(tb);

	if (!tb[IWL_MVM_VENDOR_ATTR_DBG_COLLECT_TRIGGER]) {
		err = -EINVAL;
		goto free;
	}

	trigger_desc = nla_data(tb[IWL_MVM_VENDOR_ATTR_DBG_COLLECT_TRIGGER]);
	len = nla_len(tb[IWL_MVM_VENDOR_ATTR_DBG_COLLECT_TRIGGER]);

	iwl_fw_dbg_collect(&mvm->fwrt, FW_DBG_TRIGGER_USER_EXTENDED,
			   trigger_desc, len, NULL);
	err = 0;

free:
	kfree(tb);
	return err;
}

static int iwl_mvm_vendor_nan_faw_conf(struct wiphy *wiphy,
				       struct wireless_dev *wdev,
				       const void *data, int data_len)
{
	struct nlattr **tb;
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct iwl_mvm *mvm = IWL_MAC80211_GET_MVM(hw);
	struct cfg80211_chan_def def = {};
	struct ieee80211_channel *chan;
	u32 freq;
	u8 slots;
	int err;

	tb = iwl_mvm_parse_vendor_data(data, data_len);
	if (IS_ERR(tb))
		return PTR_ERR(tb);

	if (!tb[IWL_MVM_VENDOR_ATTR_NAN_FAW_SLOTS]) {
		err = -EINVAL;
		goto free;
	}

	if (!tb[IWL_MVM_VENDOR_ATTR_NAN_FAW_FREQ]) {
		err = -EINVAL;
		goto free;
	}

	freq = nla_get_u32(tb[IWL_MVM_VENDOR_ATTR_NAN_FAW_FREQ]);
	slots = nla_get_u8(tb[IWL_MVM_VENDOR_ATTR_NAN_FAW_SLOTS]);

	chan = ieee80211_get_channel(wiphy, freq);
	if (!chan) {
		err = -EINVAL;
		goto free;
	}

	cfg80211_chandef_create(&def, chan, NL80211_CHAN_NO_HT);

	if (!cfg80211_chandef_usable(wiphy, &def, IEEE80211_CHAN_DISABLED)) {
		err = -EINVAL;
		goto free;
	}

	err = iwl_mvm_nan_config_nan_faw_cmd(mvm, &def, slots);
free:
	kfree(tb);
	return err;
}

#ifdef CONFIG_ACPI
static int iwl_mvm_vendor_set_dynamic_txp_profile(struct wiphy *wiphy,
						  struct wireless_dev *wdev,
						  const void *data,
						  int data_len)
{
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct iwl_mvm *mvm = IWL_MAC80211_GET_MVM(hw);
	struct nlattr **tb;
	u8 chain_a, chain_b;
	int err;

	tb = iwl_mvm_parse_vendor_data(data, data_len);
	if (IS_ERR(tb))
		return PTR_ERR(tb);

	if (!tb[IWL_MVM_VENDOR_ATTR_SAR_CHAIN_A_PROFILE] ||
	    !tb[IWL_MVM_VENDOR_ATTR_SAR_CHAIN_B_PROFILE]) {
		err = -EINVAL;
		goto free;
	}

	chain_a = nla_get_u8(tb[IWL_MVM_VENDOR_ATTR_SAR_CHAIN_A_PROFILE]);
	chain_b = nla_get_u8(tb[IWL_MVM_VENDOR_ATTR_SAR_CHAIN_B_PROFILE]);

	if (mvm->fwrt.sar_chain_a_profile == chain_a &&
	    mvm->fwrt.sar_chain_b_profile == chain_b) {
		err = 0;
		goto free;
	}

	mvm->fwrt.sar_chain_a_profile = chain_a;
	mvm->fwrt.sar_chain_b_profile = chain_b;

	mutex_lock(&mvm->mutex);
	if (!iwl_mvm_firmware_running(mvm)) {
		err = 0;
		goto unlock;
	} else {
		err = iwl_mvm_sar_select_profile(mvm, chain_a, chain_b);
	}

unlock:
	mutex_unlock(&mvm->mutex);
free:
	kfree(tb);
	if (err > 0)
		/*
		 * For SAR validation purpose we need to track the exact return
		 * value of iwl_mvm_sar_select_profile, mostly to differentiate
		 * between general SAR failure and the case of WRDS disable
		 * (it is illegal if WRDS doesn't exist but WGDS does).
		 * Since nl80211 forbids a positive number as a return value,
		 * in case SAR is disabled overwrite it with -ENOENT.
		 */
		err = -ENOENT;
	return err;
}

static int iwl_mvm_vendor_get_sar_profile_info(struct wiphy *wiphy,
					       struct wireless_dev *wdev,
					       const void *data,
					       int data_len)
{
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct iwl_mvm *mvm = IWL_MAC80211_GET_MVM(hw);
	struct sk_buff *skb;
	int i;
	u32 n_profiles = 0;

	for (i = 0; i < ACPI_SAR_PROFILE_NUM; i++) {
		if (mvm->fwrt.sar_profiles[i].enabled)
			n_profiles++;
	}

	skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, 100);
	if (!skb)
		return -ENOMEM;
	if (nla_put_u8(skb, IWL_MVM_VENDOR_ATTR_SAR_ENABLED_PROFILE_NUM,
		       n_profiles) ||
	    nla_put_u8(skb, IWL_MVM_VENDOR_ATTR_SAR_CHAIN_A_PROFILE,
		       mvm->fwrt.sar_chain_a_profile) ||
	    nla_put_u8(skb, IWL_MVM_VENDOR_ATTR_SAR_CHAIN_B_PROFILE,
		       mvm->fwrt.sar_chain_b_profile)) {
		kfree_skb(skb);
		return -ENOBUFS;
	}

	return cfg80211_vendor_cmd_reply(skb);
}

static int iwl_mvm_vendor_put_geo_profile(struct iwl_mvm *mvm, struct sk_buff *skb, int profile)
{
	int i;

	for (i = 0; i < ACPI_GEO_NUM_BANDS_REV2; i++) {
		struct nlattr *nl_band = nla_nest_start(skb, i + 1);

		if (!nl_band)
			return -ENOBUFS;

		nla_put_u8(skb, IWL_VENDOR_SAR_GEO_MAX_TXP,
			   mvm->fwrt.geo_profiles[profile - 1].bands[i].max);
		nla_put_u8(skb, IWL_VENDOR_SAR_GEO_CHAIN_A_OFFSET,
			   mvm->fwrt.geo_profiles[profile - 1].bands[i].chains[0]);
		nla_put_u8(skb, IWL_VENDOR_SAR_GEO_CHAIN_B_OFFSET,
			   mvm->fwrt.geo_profiles[profile - 1].bands[i].chains[1]);
		nla_nest_end(skb, nl_band);
	}
	return 0;
}

static int iwl_mvm_vendor_get_geo_profile_info(struct wiphy *wiphy,
					       struct wireless_dev *wdev,
					       const void *data,
					       int data_len)
{
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct iwl_mvm *mvm = IWL_MAC80211_GET_MVM(hw);
	struct sk_buff *skb;
	struct nlattr *nl_profile;
	int tbl_idx, ret;

	tbl_idx = iwl_mvm_get_sar_geo_profile(mvm);
	if (tbl_idx < 0)
		return tbl_idx;

	skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, 100);
	if (!skb)
		return -ENOMEM;

	nl_profile = nla_nest_start(skb, IWL_MVM_VENDOR_ATTR_SAR_GEO_PROFILE);
	if (!nl_profile) {
		kfree_skb(skb);
		return -ENOBUFS;
	}
	if (!tbl_idx)
		goto out;

	/* put into the skb the info for profile tbl_idx */
	ret = iwl_mvm_vendor_put_geo_profile(mvm, skb, tbl_idx);
	if (ret < 0) {
		kfree_skb(skb);
		return ret;
	}
out:
	nla_nest_end(skb, nl_profile);

	return cfg80211_vendor_cmd_reply(skb);
}

static int iwl_mvm_vendor_ppag_get_table(struct wiphy *wiphy,
					 struct wireless_dev *wdev,
					 const void *data,
					 int data_len)
{
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct iwl_mvm *mvm = IWL_MAC80211_GET_MVM(hw);
	struct sk_buff *skb = NULL;
	struct nlattr *nl_table;
	int ret, per_chain_size, chain;

	/* if ppag is disabled */
	if (!mvm->fwrt.ppag_flags)
		return -ENOENT;

	skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, 180);
	if (!skb)
		return -ENOMEM;

	nl_table = nla_nest_start(skb, IWL_MVM_VENDOR_ATTR_PPAG_TABLE |
				   NLA_F_NESTED);
	if (!nl_table) {
		ret = -ENOBUFS;
		goto err;
	}

	per_chain_size = (mvm->fwrt.ppag_ver == 0) ?
		IWL_NUM_SUB_BANDS_V1 : IWL_NUM_SUB_BANDS_V2;

	for (chain = 0; chain < IWL_NUM_CHAIN_LIMITS; chain++) {
		if (nla_put(skb, chain + 1, per_chain_size,
			    &mvm->fwrt.ppag_chains[chain].subbands[0])) {
			ret = -ENOBUFS;
			goto err;
		}
	}

	nla_nest_end(skb, nl_table);

	/* put the ppag version */
	if (nla_put_u32(skb, IWL_MVM_VENDOR_ATTR_PPAG_NUM_SUB_BANDS,
			per_chain_size)) {
		ret = -ENOBUFS;
		goto err;
	}

	return cfg80211_vendor_cmd_reply(skb);
err:
	kfree_skb(skb);
	return ret;
}

static int iwl_mvm_vendor_sar_get_table(struct wiphy *wiphy,
					struct wireless_dev *wdev,
					const void *data,
					int data_len)
{
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct iwl_mvm *mvm = IWL_MAC80211_GET_MVM(hw);
	struct sk_buff *skb = NULL;
	struct nlattr *nl_table;
	int prof, chain, ret, fw_ver;

	/* if wrds is disabled - ewrd must be disabled too */
	if (!mvm->fwrt.sar_profiles[0].enabled)
		return -ENOENT;

	skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, 100);
	if (!skb)
		return -ENOMEM;

	nl_table = nla_nest_start(skb, IWL_MVM_VENDOR_ATTR_SAR_TABLE);
	if (!nl_table) {
		kfree_skb(skb);
		return -ENOBUFS;
	}

	for (prof = 0; prof < ACPI_SAR_PROFILE_NUM; prof++) {
		struct nlattr *nl_profile;

		if (!mvm->fwrt.sar_profiles[prof].enabled)
			break;

		nl_profile = nla_nest_start(skb, prof + 1);
		if (!nl_profile) {
			ret = -ENOBUFS;
			goto err;
		}

		/* put info per chain */
		for (chain = 0; chain < ACPI_SAR_NUM_CHAINS_REV2; chain++) {
			if (nla_put(skb, chain + 1, ACPI_SAR_NUM_SUB_BANDS_REV2,
				    mvm->fwrt.sar_profiles[prof].chains[chain].subbands)) {
				ret = -ENOBUFS;
				goto err;
			}
		}

		nla_nest_end(skb, nl_profile);
	}
	nla_nest_end(skb, nl_table);

	fw_ver = iwl_fw_lookup_cmd_ver(mvm->fw, REDUCE_TX_POWER_CMD,
				       IWL_FW_CMD_VER_UNKNOWN);

	if (nla_put_u32(skb, IWL_MVM_VENDOR_ATTR_SAR_VER, fw_ver)) {
		ret = -ENOBUFS;
		goto err;
	}
	return cfg80211_vendor_cmd_reply(skb);
err:
	kfree_skb(skb);
	return ret;
}

static int iwl_mvm_vendor_geo_sar_get_table(struct wiphy *wiphy,
					    struct wireless_dev *wdev,
					    const void *data,
					    int data_len)
{
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct iwl_mvm *mvm = IWL_MAC80211_GET_MVM(hw);
	struct sk_buff *skb = NULL;
	struct nlattr *nl_table;
	int i, ret;

	if (!mvm->fwrt.geo_enabled)
		return -ENOENT;

	skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, 100);
	if (!skb)
		return -ENOMEM;

	nl_table = nla_nest_start(skb, IWL_MVM_VENDOR_ATTR_GEO_SAR_TABLE);
	if (!nl_table) {
		ret = -ENOBUFS;
		goto err;
	}

	/* get each profile */
	for (i = 0; i < ACPI_NUM_GEO_PROFILES; i++) {
		struct nlattr *nl_profile = nla_nest_start(skb, i + 1);

		if (!nl_profile) {
			ret = -ENOBUFS;
			goto err;
		}

		/* put into the skb the info for profile i+1
		 * (we don't have profile 0) */
		ret = iwl_mvm_vendor_put_geo_profile(mvm, skb, i + 1);
		if (ret < 0) {
			ret = -ENOBUFS;
			goto err;
		}
		nla_nest_end(skb, nl_profile);
	}
	nla_nest_end(skb, nl_table);

	if (nla_put_u32(skb, IWL_MVM_VENDOR_ATTR_GEO_SAR_VER, mvm->fwrt.geo_rev)) {
		ret = -ENOBUFS;
		goto err;
	}

	return cfg80211_vendor_cmd_reply(skb);
err:
	kfree_skb(skb);
	return ret;
}

static int iwl_mvm_vendor_sgom_get_table(struct wiphy *wiphy,
					 struct wireless_dev *wdev,
					 const void *data,
					 int data_len)
{
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct iwl_mvm *mvm = IWL_MAC80211_GET_MVM(hw);
	struct sk_buff *skb;
	u8 *table;
	int size, ret = 0;

	if (!mvm->fwrt.sgom_enabled)
		return -ENOENT;

	size = sizeof(mvm->fwrt.sgom_table.offset_map);
	skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, size + 50);
	if (!skb)
		return -ENOMEM;

	table = mvm->fwrt.sgom_table.offset_map[0];
	if (nla_put(skb, IWL_MVM_VENDOR_ATTR_SGOM_TABLE, size, table)) {
		ret = -ENOBUFS;
		goto err;
	}

	return cfg80211_vendor_cmd_reply(skb);
err:
	kfree_skb(skb);
	return ret;
}

#endif

static const struct nla_policy
iwl_mvm_vendor_fips_hw_policy[NUM_IWL_VENDOR_FIPS_TEST_VECTOR_HW] = {
	[IWL_VENDOR_FIPS_TEST_VECTOR_HW_KEY] = { .type = NLA_BINARY },
	[IWL_VENDOR_FIPS_TEST_VECTOR_HW_NONCE] = { .type = NLA_BINARY },
	[IWL_VENDOR_FIPS_TEST_VECTOR_HW_AAD] = { .type = NLA_BINARY },
	[IWL_VENDOR_FIPS_TEST_VECTOR_HW_PAYLOAD] = { .type = NLA_BINARY },
	[IWL_VENDOR_FIPS_TEST_VECTOR_HW_FLAGS] = { .type = NLA_U8 },
};

static int iwl_mvm_vendor_validate_ccm_vector(struct nlattr **tb)
{
	if (!tb[IWL_VENDOR_FIPS_TEST_VECTOR_HW_KEY] ||
	    !tb[IWL_VENDOR_FIPS_TEST_VECTOR_HW_NONCE] ||
	    !tb[IWL_VENDOR_FIPS_TEST_VECTOR_HW_AAD] ||
	    nla_len(tb[IWL_VENDOR_FIPS_TEST_VECTOR_HW_KEY]) !=
	    FIPS_KEY_LEN_128 ||
	    nla_len(tb[IWL_VENDOR_FIPS_TEST_VECTOR_HW_NONCE]) !=
	    FIPS_CCM_NONCE_LEN)
		return -EINVAL;

	return 0;
}

static int iwl_mvm_vendor_validate_gcm_vector(struct nlattr **tb)
{
	if (!tb[IWL_VENDOR_FIPS_TEST_VECTOR_HW_KEY] ||
	    !tb[IWL_VENDOR_FIPS_TEST_VECTOR_HW_NONCE] ||
	    !tb[IWL_VENDOR_FIPS_TEST_VECTOR_HW_AAD] ||
	    (nla_len(tb[IWL_VENDOR_FIPS_TEST_VECTOR_HW_KEY]) !=
	     FIPS_KEY_LEN_128 &&
	     nla_len(tb[IWL_VENDOR_FIPS_TEST_VECTOR_HW_KEY]) !=
	     FIPS_KEY_LEN_256) ||
	    nla_len(tb[IWL_VENDOR_FIPS_TEST_VECTOR_HW_NONCE]) !=
	    FIPS_GCM_NONCE_LEN)
		return -EINVAL;

	return 0;
}

static int iwl_mvm_vendor_validate_aes_vector(struct nlattr **tb)
{
	if (!tb[IWL_VENDOR_FIPS_TEST_VECTOR_HW_KEY] ||
	    (nla_len(tb[IWL_VENDOR_FIPS_TEST_VECTOR_HW_KEY]) !=
	     FIPS_KEY_LEN_128 &&
	     nla_len(tb[IWL_VENDOR_FIPS_TEST_VECTOR_HW_KEY]) !=
	     FIPS_KEY_LEN_256))
		return -EINVAL;

	return 0;
}

/**
 * iwl_mvm_vendor_build_vector - build FIPS test vector for AES/CCM/GCM tests
 *
 * @cmd_buf: the command buffer is returned by this pointer in case of success.
 * @vector: test vector attributes.
 * @flags: specifies which encryption algorithm to use. One of
 *	&IWL_FIPS_TEST_VECTOR_FLAGS_CCM, &IWL_FIPS_TEST_VECTOR_FLAGS_GCM and
 *	&IWL_FIPS_TEST_VECTOR_FLAGS_AES.
 *
 * This function returns the length of the command buffer (in bytes) in case of
 * success, or a negative error code on failure.
 */
static int iwl_mvm_vendor_build_vector(u8 **cmd_buf, struct nlattr *vector,
				       u8 flags)
{
	struct nlattr *tb[NUM_IWL_VENDOR_FIPS_TEST_VECTOR_HW];
	struct iwl_fips_test_cmd *cmd;
	int err;
	int payload_len = 0;
	u8 *buf;

	err = nla_parse_nested(tb, MAX_IWL_VENDOR_FIPS_TEST_VECTOR_HW,
			       vector, iwl_mvm_vendor_fips_hw_policy, NULL);
	if (err)
		return err;

	switch (flags) {
	case IWL_FIPS_TEST_VECTOR_FLAGS_CCM:
		err = iwl_mvm_vendor_validate_ccm_vector(tb);
		break;
	case IWL_FIPS_TEST_VECTOR_FLAGS_GCM:
		err = iwl_mvm_vendor_validate_gcm_vector(tb);
		break;
	case IWL_FIPS_TEST_VECTOR_FLAGS_AES:
		err = iwl_mvm_vendor_validate_aes_vector(tb);
		break;
	default:
		return -EINVAL;
	}

	if (err)
		return err;

	if (tb[IWL_VENDOR_FIPS_TEST_VECTOR_HW_AAD] &&
	    nla_len(tb[IWL_VENDOR_FIPS_TEST_VECTOR_HW_AAD]) > FIPS_MAX_AAD_LEN)
		return -EINVAL;

	if (tb[IWL_VENDOR_FIPS_TEST_VECTOR_HW_PAYLOAD])
		payload_len =
			nla_len(tb[IWL_VENDOR_FIPS_TEST_VECTOR_HW_PAYLOAD]);

	buf = kzalloc(sizeof(*cmd) + payload_len, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	cmd = (void *)buf;

	cmd->flags = cpu_to_le32(flags);

	memcpy(cmd->key, nla_data(tb[IWL_VENDOR_FIPS_TEST_VECTOR_HW_KEY]),
	       nla_len(tb[IWL_VENDOR_FIPS_TEST_VECTOR_HW_KEY]));

	if (nla_len(tb[IWL_VENDOR_FIPS_TEST_VECTOR_HW_KEY]) == FIPS_KEY_LEN_256)
		cmd->flags |= cpu_to_le32(IWL_FIPS_TEST_VECTOR_FLAGS_KEY_256);

	if (tb[IWL_VENDOR_FIPS_TEST_VECTOR_HW_NONCE])
		memcpy(cmd->nonce,
		       nla_data(tb[IWL_VENDOR_FIPS_TEST_VECTOR_HW_NONCE]),
		       nla_len(tb[IWL_VENDOR_FIPS_TEST_VECTOR_HW_NONCE]));

	if (tb[IWL_VENDOR_FIPS_TEST_VECTOR_HW_AAD]) {
		memcpy(cmd->aad,
		       nla_data(tb[IWL_VENDOR_FIPS_TEST_VECTOR_HW_AAD]),
		       nla_len(tb[IWL_VENDOR_FIPS_TEST_VECTOR_HW_AAD]));
		cmd->aad_len =
			cpu_to_le32(nla_len(tb[IWL_VENDOR_FIPS_TEST_VECTOR_HW_AAD]));
	}

	if (payload_len) {
		memcpy(cmd->payload,
		       nla_data(tb[IWL_VENDOR_FIPS_TEST_VECTOR_HW_PAYLOAD]),
		       payload_len);
		cmd->payload_len = cpu_to_le32(payload_len);
	}

	if (tb[IWL_VENDOR_FIPS_TEST_VECTOR_HW_FLAGS]) {
		u8 hw_flags =
			nla_get_u8(tb[IWL_VENDOR_FIPS_TEST_VECTOR_HW_FLAGS]);

		if (hw_flags & IWL_VENDOR_FIPS_TEST_VECTOR_FLAGS_ENCRYPT)
			cmd->flags |=
				cpu_to_le32(IWL_FIPS_TEST_VECTOR_FLAGS_ENC);
	}

	*cmd_buf = buf;
	return sizeof(*cmd) + payload_len;
}

static int iwl_mvm_vendor_test_fips_send_resp(struct wiphy *wiphy,
					      struct iwl_fips_test_resp *resp)
{
	struct sk_buff *skb;
	u32 resp_len = le32_to_cpu(resp->len);
	u32 *status = (void *)(resp->payload + resp_len);

	skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, sizeof(*resp));
	if (!skb)
		return -ENOMEM;

	if ((*status) == IWL_FIPS_TEST_STATUS_SUCCESS &&
	    nla_put(skb, IWL_MVM_VENDOR_ATTR_FIPS_TEST_RESULT, resp_len,
		    resp->payload)) {
		kfree_skb(skb);
		return -ENOBUFS;
	}

	return cfg80211_vendor_cmd_reply(skb);
}

static int iwl_mvm_vendor_test_fips(struct wiphy *wiphy,
				    struct wireless_dev *wdev,
				    const void *data, int data_len)
{
	struct nlattr **tb;
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct iwl_mvm *mvm = IWL_MAC80211_GET_MVM(hw);
	struct iwl_host_cmd hcmd = {
		.id = WIDE_ID(LEGACY_GROUP, FIPS_TEST_VECTOR_CMD),
		.flags = CMD_WANT_SKB,
		.dataflags = { IWL_HCMD_DFL_NOCOPY },
	};
	struct iwl_rx_packet *pkt;
	struct iwl_fips_test_resp *resp;
	struct nlattr *vector;
	u8 flags;
	u8 *buf = NULL;
	int ret;

	tb = iwl_mvm_parse_vendor_data(data, data_len);
	if (IS_ERR(tb))
		return PTR_ERR(tb);

	if (tb[IWL_MVM_VENDOR_ATTR_FIPS_TEST_VECTOR_HW_CCM]) {
		vector = tb[IWL_MVM_VENDOR_ATTR_FIPS_TEST_VECTOR_HW_CCM];
		flags = IWL_FIPS_TEST_VECTOR_FLAGS_CCM;
	} else if (tb[IWL_MVM_VENDOR_ATTR_FIPS_TEST_VECTOR_HW_GCM]) {
		vector = tb[IWL_MVM_VENDOR_ATTR_FIPS_TEST_VECTOR_HW_GCM];
		flags = IWL_FIPS_TEST_VECTOR_FLAGS_GCM;
	} else if (tb[IWL_MVM_VENDOR_ATTR_FIPS_TEST_VECTOR_HW_AES]) {
		vector = tb[IWL_MVM_VENDOR_ATTR_FIPS_TEST_VECTOR_HW_AES];
		flags = IWL_FIPS_TEST_VECTOR_FLAGS_AES;
	} else {
		ret = -EINVAL;
		goto free;
	}

	ret = iwl_mvm_vendor_build_vector(&buf, vector, flags);
	if (ret <= 0)
		goto free;

	hcmd.data[0] = buf;
	hcmd.len[0] = ret;

	mutex_lock(&mvm->mutex);
	ret = iwl_mvm_send_cmd(mvm, &hcmd);
	mutex_unlock(&mvm->mutex);

	if (ret)
		goto free;

	pkt = hcmd.resp_pkt;
	resp = (void *)pkt->data;

	iwl_mvm_vendor_test_fips_send_resp(wiphy, resp);
	iwl_free_resp(&hcmd);

free:
	kfree(buf);
	kfree(tb);
	return ret;
}

static int iwl_mvm_vendor_csi_register(struct wiphy *wiphy,
				       struct wireless_dev *wdev,
				       const void *data, int data_len)
{
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct iwl_mvm *mvm = IWL_MAC80211_GET_MVM(hw);

	mvm->csi_portid = cfg80211_vendor_cmd_get_sender(wiphy);

	return 0;
}

static int iwl_mvm_vendor_add_pasn_sta(struct wiphy *wiphy,
				       struct wireless_dev *wdev,
				       const void *data, int data_len)
{
	struct nlattr **tb;
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct iwl_mvm *mvm = IWL_MAC80211_GET_MVM(hw);
	struct ieee80211_vif *vif = wdev_to_ieee80211_vif(wdev);
	u8 *addr, *tk = NULL, *hltk;
	u32 tk_len = 0, hltk_len, cipher;
	int ret = 0;
	struct ieee80211_sta *sta;

	if (!vif)
		return -ENODEV;

	tb = iwl_mvm_parse_vendor_data(data, data_len);
	if (IS_ERR(tb))
		return PTR_ERR(tb);

	if (!tb[IWL_MVM_VENDOR_ATTR_ADDR] ||
	    !tb[IWL_MVM_VENDOR_ATTR_STA_HLTK] ||
	    !tb[IWL_MVM_VENDOR_ATTR_STA_CIPHER])
		return -EINVAL;

	addr = nla_data(tb[IWL_MVM_VENDOR_ATTR_ADDR]);
	cipher = nla_get_u32(tb[IWL_MVM_VENDOR_ATTR_STA_CIPHER]);
	hltk = nla_data(tb[IWL_MVM_VENDOR_ATTR_STA_HLTK]);
	hltk_len = nla_len(tb[IWL_MVM_VENDOR_ATTR_STA_HLTK]);

	rcu_read_lock();
	sta = ieee80211_find_sta(vif, addr);
	if ((!tb[IWL_MVM_VENDOR_ATTR_STA_TK] && (!sta || !sta->mfp)) ||
	    (tb[IWL_MVM_VENDOR_ATTR_STA_TK] && sta && sta->mfp))
		ret = -EINVAL;
	rcu_read_unlock();
	if (ret)
		return ret;

	if (tb[IWL_MVM_VENDOR_ATTR_STA_TK]) {
		tk = nla_data(tb[IWL_MVM_VENDOR_ATTR_STA_TK]);
		tk_len = nla_len(tb[IWL_MVM_VENDOR_ATTR_STA_TK]);
	}

	mutex_lock(&mvm->mutex);
	if (vif->bss_conf.ftm_responder)
		ret = iwl_mvm_ftm_respoder_add_pasn_sta(mvm, vif, addr, cipher,
							tk, tk_len, hltk,
							hltk_len);
	else
		iwl_mvm_ftm_add_pasn_sta(mvm, vif, addr, cipher, tk, tk_len,
					 hltk, hltk_len);
	mutex_unlock(&mvm->mutex);
	return ret;
}

static int iwl_mvm_vendor_remove_pasn_sta(struct wiphy *wiphy,
					  struct wireless_dev *wdev,
					  const void *data, int data_len)
{
	struct nlattr **tb;
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct iwl_mvm *mvm = IWL_MAC80211_GET_MVM(hw);
	struct ieee80211_vif *vif = wdev_to_ieee80211_vif(wdev);
	u8 *addr;
	int ret = 0;

	if (!vif)
		return -ENODEV;

	tb = iwl_mvm_parse_vendor_data(data, data_len);
	if (IS_ERR(tb))
		return PTR_ERR(tb);

	if (!tb[IWL_MVM_VENDOR_ATTR_ADDR])
		return -EINVAL;

	addr = nla_data(tb[IWL_MVM_VENDOR_ATTR_ADDR]);

	mutex_lock(&mvm->mutex);
	if (vif->bss_conf.ftm_responder)
		ret = iwl_mvm_ftm_resp_remove_pasn_sta(mvm, vif, addr);
	else
		iwl_mvm_ftm_remove_pasn_sta(mvm, addr);
	mutex_unlock(&mvm->mutex);
	return ret;
}

static int iwl_mvm_time_sync_measurement_config(struct wiphy *wiphy,
						struct wireless_dev *wdev,
						const void *data, int data_len)
{
	struct nlattr **tb;
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct iwl_mvm *mvm = IWL_MAC80211_GET_MVM(hw);
	struct iwl_time_sync_cfg_cmd cmd = {};
	u32 protocol_types;
	int err;

	tb = iwl_mvm_parse_vendor_data(data, data_len);
	if (IS_ERR(tb))
		return PTR_ERR(tb);

	if (!tb[IWL_MVM_VENDOR_ATTR_ADDR] ||
	    !tb[IWL_MVM_VENDOR_ATTR_TIME_SYNC_PROTOCOL_TYPE])
		return -EINVAL;

	ether_addr_copy(cmd.peer_addr, nla_data(tb[IWL_MVM_VENDOR_ATTR_ADDR]));

	protocol_types = nla_get_u32(tb[IWL_MVM_VENDOR_ATTR_TIME_SYNC_PROTOCOL_TYPE]);

	/* Check if the requested configuration was already set earlier */
	if (protocol_types == mvm->time_msmt_cfg)
		return -EALREADY;

	if (protocol_types <= (IWL_MVM_VENDOR_TIME_SYNC_PROTOCOL_TM |
			      IWL_MVM_VENDOR_TIME_SYNC_PROTOCOL_FTM))
		cmd.protocols = cpu_to_le32(protocol_types);
	else
		return -EINVAL;

	mutex_lock(&mvm->mutex);
	err = iwl_mvm_send_cmd_pdu(mvm,
				   WIDE_ID(DATA_PATH_GROUP,
					   WNM_80211V_TIMING_MEASUREMENT_CONFIG_CMD),
				   0, sizeof(cmd), &cmd);
	mutex_unlock(&mvm->mutex);

	if (err) {
		IWL_ERR(mvm, "Failed to send TM/FTM Measurement cfg cmd: %d\n", err);
		return err;
	}

	/* Save the changed time sync measurement configuration */
	mvm->time_msmt_cfg = protocol_types;
	ether_addr_copy(mvm->time_msmt_peer_addr, cmd.peer_addr);
	mvm->time_sync_wdev = wdev;

	return 0;
}

static int iwl_mvm_vendor_get_csme_conn_info(struct wiphy *wiphy,
					     struct wireless_dev *wdev,
					     const void *data, int data_len)
{
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct iwl_mvm *mvm = IWL_MAC80211_GET_MVM(hw);
	struct iwl_mvm_csme_conn_info *csme_conn_info;
	struct sk_buff *skb;
	int err = 0;

	mutex_lock(&mvm->mutex);
	csme_conn_info = iwl_mvm_get_csme_conn_info(mvm);

	if (!csme_conn_info) {
		err = -EINVAL;
		goto out_unlock;
	}

	skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, 200);
	if (!skb) {
		err = -ENOMEM;
		goto out_unlock;
	}

	if (nla_put_u32(skb, IWL_MVM_VENDOR_ATTR_AUTH_MODE,
			csme_conn_info->conn_info.auth_mode) ||
	    nla_put(skb, IWL_MVM_VENDOR_ATTR_SSID,
		    csme_conn_info->conn_info.ssid_len,
		    csme_conn_info->conn_info.ssid) ||
	    nla_put_u32(skb, IWL_MVM_VENDOR_ATTR_STA_CIPHER,
			csme_conn_info->conn_info.pairwise_cipher) ||
	    nla_put_u8(skb, IWL_MVM_VENDOR_ATTR_CHANNEL_NUM,
		       csme_conn_info->conn_info.channel) ||
	    nla_put(skb, IWL_MVM_VENDOR_ATTR_ADDR, ETH_ALEN,
		    csme_conn_info->conn_info.bssid)) {
		kfree_skb(skb);
		err = -ENOBUFS;
	}

out_unlock:
	mutex_unlock(&mvm->mutex);
	if (err)
		return err;

	return cfg80211_vendor_cmd_reply(skb);
}

static int iwl_mvm_vendor_host_get_ownership(struct wiphy *wiphy,
					     struct wireless_dev *wdev,
					     const void *data, int data_len)
{
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct iwl_mvm *mvm = IWL_MAC80211_GET_MVM(hw);

	mutex_lock(&mvm->mutex);
	iwl_mvm_mei_get_ownership(mvm);
	mutex_unlock(&mvm->mutex);

	return 0;
}

static const struct wiphy_vendor_command iwl_mvm_vendor_commands[] = {
	{
		.info = {
			.vendor_id = INTEL_OUI,
			.subcmd = IWL_MVM_VENDOR_CMD_SET_LOW_LATENCY,
		},
		.flags = WIPHY_VENDOR_CMD_NEED_NETDEV |
			 WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = iwl_mvm_set_low_latency,
		.policy = iwl_mvm_vendor_attr_policy,
		.maxattr = MAX_IWL_MVM_VENDOR_ATTR,
	},
	{
		.info = {
			.vendor_id = INTEL_OUI,
			.subcmd = IWL_MVM_VENDOR_CMD_GET_LOW_LATENCY,
		},
		.flags = WIPHY_VENDOR_CMD_NEED_NETDEV |
			 WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = iwl_mvm_get_low_latency,
		.policy = iwl_mvm_vendor_attr_policy,
		.maxattr = MAX_IWL_MVM_VENDOR_ATTR,
	},
	{
		.info = {
			.vendor_id = INTEL_OUI,
			.subcmd = IWL_MVM_VENDOR_CMD_SET_COUNTRY,
		},
		.flags = WIPHY_VENDOR_CMD_NEED_NETDEV |
			 WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = iwl_mvm_set_country,
		.policy = iwl_mvm_vendor_attr_policy,
		.maxattr = MAX_IWL_MVM_VENDOR_ATTR,
	},
#ifdef CPTCFG_IWLMVM_TDLS_PEER_CACHE
	{
		.info = {
			.vendor_id = INTEL_OUI,
			.subcmd = IWL_MVM_VENDOR_CMD_TDLS_PEER_CACHE_ADD,
		},
		.flags = WIPHY_VENDOR_CMD_NEED_NETDEV |
			 WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = iwl_vendor_tdls_peer_cache_add,
		.policy = iwl_mvm_vendor_attr_policy,
		.maxattr = MAX_IWL_MVM_VENDOR_ATTR,
	},
	{
		.info = {
			.vendor_id = INTEL_OUI,
			.subcmd = IWL_MVM_VENDOR_CMD_TDLS_PEER_CACHE_DEL,
		},
		.flags = WIPHY_VENDOR_CMD_NEED_NETDEV |
			 WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = iwl_vendor_tdls_peer_cache_del,
		.policy = iwl_mvm_vendor_attr_policy,
		.maxattr = MAX_IWL_MVM_VENDOR_ATTR,
	},
	{
		.info = {
			.vendor_id = INTEL_OUI,
			.subcmd = IWL_MVM_VENDOR_CMD_TDLS_PEER_CACHE_QUERY,
		},
		.flags = WIPHY_VENDOR_CMD_NEED_NETDEV |
			 WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = iwl_vendor_tdls_peer_cache_query,
		.policy = iwl_mvm_vendor_attr_policy,
		.maxattr = MAX_IWL_MVM_VENDOR_ATTR,
	},
#endif /* CPTCFG_IWLMVM_TDLS_PEER_CACHE */
	{
		.info = {
			.vendor_id = INTEL_OUI,
			.subcmd = IWL_MVM_VENDOR_CMD_SET_NIC_TXPOWER_LIMIT,
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV,
		.doit = iwl_vendor_set_nic_txpower_limit,
		.policy = iwl_mvm_vendor_attr_policy,
		.maxattr = MAX_IWL_MVM_VENDOR_ATTR,
	},
#ifdef CPTCFG_IWLMVM_P2P_OPPPS_TEST_WA
	{
		.info = {
			.vendor_id = INTEL_OUI,
			.subcmd = IWL_MVM_VENDOR_CMD_OPPPS_WA,
		},
		.flags = WIPHY_VENDOR_CMD_NEED_NETDEV |
			 WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = iwl_mvm_oppps_wa,
		.policy = iwl_mvm_vendor_attr_policy,
		.maxattr = MAX_IWL_MVM_VENDOR_ATTR,
	},
#endif
	{
		.info = {
			.vendor_id = INTEL_OUI,
			.subcmd = IWL_MVM_VENDOR_CMD_RXFILTER,
		},
		.flags = WIPHY_VENDOR_CMD_NEED_NETDEV |
			 WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = iwl_mvm_vendor_rxfilter,
		.policy = iwl_mvm_vendor_attr_policy,
		.maxattr = MAX_IWL_MVM_VENDOR_ATTR,
	},
	{
		.info = {
			.vendor_id = INTEL_OUI,
			.subcmd = IWL_MVM_VENDOR_CMD_DBG_COLLECT,
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			 WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = iwl_mvm_vendor_dbg_collect,
		.policy = iwl_mvm_vendor_attr_policy,
		.maxattr = MAX_IWL_MVM_VENDOR_ATTR,
	},
	{
		.info = {
			.vendor_id = INTEL_OUI,

			.subcmd = IWL_MVM_VENDOR_CMD_NAN_FAW_CONF,
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			 WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = iwl_mvm_vendor_nan_faw_conf,
		.policy = iwl_mvm_vendor_attr_policy,
		.maxattr = MAX_IWL_MVM_VENDOR_ATTR,
	},
#ifdef CONFIG_ACPI
	{
		.info = {
			.vendor_id = INTEL_OUI,
			.subcmd = IWL_MVM_VENDOR_CMD_SET_SAR_PROFILE,
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV,
		.doit = iwl_mvm_vendor_set_dynamic_txp_profile,
		.policy = iwl_mvm_vendor_attr_policy,
		.maxattr = MAX_IWL_MVM_VENDOR_ATTR,
	},
	{
		.info = {
			.vendor_id = INTEL_OUI,
			.subcmd = IWL_MVM_VENDOR_CMD_GET_SAR_PROFILE_INFO,
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV,
		.doit = iwl_mvm_vendor_get_sar_profile_info,
		.policy = iwl_mvm_vendor_attr_policy,
		.maxattr = MAX_IWL_MVM_VENDOR_ATTR,
	},
	{
		.info = {
			.vendor_id = INTEL_OUI,
			.subcmd = IWL_MVM_VENDOR_CMD_GET_SAR_GEO_PROFILE,
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			 WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = iwl_mvm_vendor_get_geo_profile_info,
		.policy = iwl_mvm_vendor_attr_policy,
		.maxattr = MAX_IWL_MVM_VENDOR_ATTR,
	},
	{
		.info = {
			.vendor_id = INTEL_OUI,
			.subcmd = IWL_MVM_VENDOR_CMD_PPAG_GET_TABLE,
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV,
		.doit = iwl_mvm_vendor_ppag_get_table,
		.policy = iwl_mvm_vendor_attr_policy,
		.maxattr = MAX_IWL_MVM_VENDOR_ATTR,
	},
	{
		.info = {
			.vendor_id = INTEL_OUI,
			.subcmd = IWL_MVM_VENDOR_CMD_SAR_GET_TABLE,
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV,
		.doit = iwl_mvm_vendor_sar_get_table,
		.policy = iwl_mvm_vendor_attr_policy,
		.maxattr = MAX_IWL_MVM_VENDOR_ATTR,
	},
	{
		.info = {
			.vendor_id = INTEL_OUI,
			.subcmd = IWL_MVM_VENDOR_CMD_GEO_SAR_GET_TABLE,
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV,
		.doit = iwl_mvm_vendor_geo_sar_get_table,
		.policy = iwl_mvm_vendor_attr_policy,
		.maxattr = MAX_IWL_MVM_VENDOR_ATTR,
	},
	{
		.info = {
			.vendor_id = INTEL_OUI,
			.subcmd = IWL_MVM_VENDOR_CMD_SGOM_GET_TABLE,
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV,
		.doit = iwl_mvm_vendor_sgom_get_table,
		.policy = iwl_mvm_vendor_attr_policy,
		.maxattr = MAX_IWL_MVM_VENDOR_ATTR,
	},
#endif
	{
		.info = {
			.vendor_id = INTEL_OUI,
			.subcmd = IWL_MVM_VENDOR_CMD_TEST_FIPS,
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			 WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = iwl_mvm_vendor_test_fips,
		.policy = iwl_mvm_vendor_fips_hw_policy,
		.maxattr = MAX_IWL_VENDOR_FIPS_TEST_VECTOR_HW,
	},
	{
		.info = {
			.vendor_id = INTEL_OUI,
			.subcmd = IWL_MVM_VENDOR_CMD_CSI_EVENT,
		},
		.doit = iwl_mvm_vendor_csi_register,
		.policy = iwl_mvm_vendor_attr_policy,
		.maxattr = MAX_IWL_MVM_VENDOR_ATTR,
	},
	{
		.info = {
			.vendor_id = INTEL_OUI,
			.subcmd = IWL_MVM_VENDOR_CMD_ADD_PASN_STA,
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			 WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = iwl_mvm_vendor_add_pasn_sta,
		.policy = iwl_mvm_vendor_attr_policy,
		.maxattr = MAX_IWL_MVM_VENDOR_ATTR,
	},
	{
		.info = {
			.vendor_id = INTEL_OUI,
			.subcmd = IWL_MVM_VENDOR_CMD_REMOVE_PASN_STA,
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			 WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = iwl_mvm_vendor_remove_pasn_sta,
		.policy = iwl_mvm_vendor_attr_policy,
		.maxattr = MAX_IWL_MVM_VENDOR_ATTR,
	},
	{
		.info = {
			.vendor_id = INTEL_OUI,
			.subcmd = IWL_MVM_VENDOR_CMD_RFIM_SET_TABLE,
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			 WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = iwl_vendor_rfim_set_table,
		.policy = iwl_mvm_vendor_attr_policy,
		.maxattr = MAX_IWL_MVM_VENDOR_ATTR,
	},
	{
		.info = {
			.vendor_id = INTEL_OUI,
			.subcmd = IWL_MVM_VENDOR_CMD_RFIM_GET_TABLE,
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV,
		.doit = iwl_vendor_rfim_get_table,
		.policy = iwl_mvm_vendor_attr_policy,
		.maxattr = MAX_IWL_MVM_VENDOR_ATTR,
	},
	{
		.info = {
			.vendor_id = INTEL_OUI,
			.subcmd = IWL_MVM_VENDOR_CMD_RFIM_GET_CAPA,
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV,
		.doit = iwl_vendor_rfim_get_capa,
		.policy = iwl_mvm_vendor_attr_policy,
		.maxattr = MAX_IWL_MVM_VENDOR_ATTR,
	},
	{
		.info = {
			.vendor_id = INTEL_OUI,
			.subcmd = IWL_MVM_VENDOR_CMD_TIME_SYNC_MEASUREMENT_CONFIG,
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			 WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = iwl_mvm_time_sync_measurement_config,
		.policy = iwl_mvm_vendor_attr_policy,
		.maxattr = MAX_IWL_MVM_VENDOR_ATTR,
	},
	{
		.info = {
			.vendor_id = INTEL_OUI,
			.subcmd = IWL_MVM_VENDOR_CMD_GET_CSME_CONN_INFO,
		},
		.doit = iwl_mvm_vendor_get_csme_conn_info,
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV,
		.policy = iwl_mvm_vendor_attr_policy,
		.maxattr = MAX_IWL_MVM_VENDOR_ATTR,
	},
	{
		.info = {
			.vendor_id = INTEL_OUI,
			.subcmd = IWL_MVM_VENDOR_CMD_HOST_GET_OWNERSHIP,
		},
		.doit = iwl_mvm_vendor_host_get_ownership,
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV,
		.policy = iwl_mvm_vendor_attr_policy,
		.maxattr = MAX_IWL_MVM_VENDOR_ATTR,
	},
};

enum iwl_mvm_vendor_events_idx {
	/* 0x0 is deprecated */
	IWL_MVM_VENDOR_EVENT_IDX_CSI = 1,
	IWL_MVM_VENDOR_EVENT_IDX_TSM_CFM,
	IWL_MVM_VENDOR_EVENT_IDX_TSM_MSMT,
	IWL_MVM_VENDOR_EVENT_IDX_ROAMING_FORBIDDEN,
	NUM_IWL_MVM_VENDOR_EVENT_IDX
};

static const struct nl80211_vendor_cmd_info
iwl_mvm_vendor_events[NUM_IWL_MVM_VENDOR_EVENT_IDX] = {
	[IWL_MVM_VENDOR_EVENT_IDX_CSI] = {
		.vendor_id = INTEL_OUI,
		.subcmd = IWL_MVM_VENDOR_CMD_CSI_EVENT,
	},
	[IWL_MVM_VENDOR_EVENT_IDX_TSM_CFM] = {
		.vendor_id = INTEL_OUI,
		.subcmd = IWL_MVM_VENDOR_CMD_TIME_SYNC_MSMT_CFM_EVENT,
	},
	[IWL_MVM_VENDOR_EVENT_IDX_TSM_MSMT] = {
		.vendor_id = INTEL_OUI,
		.subcmd = IWL_MVM_VENDOR_CMD_TIME_SYNC_MSMT_EVENT,
	},
	[IWL_MVM_VENDOR_EVENT_IDX_ROAMING_FORBIDDEN] = {
		.vendor_id = INTEL_OUI,
		.subcmd = IWL_MVM_VENDOR_CMD_ROAMING_FORBIDDEN_EVENT,
	},
};

void iwl_mvm_vendor_cmds_register(struct iwl_mvm *mvm)
{
	mvm->hw->wiphy->vendor_commands = iwl_mvm_vendor_commands;
	mvm->hw->wiphy->n_vendor_commands = ARRAY_SIZE(iwl_mvm_vendor_commands);
	mvm->hw->wiphy->vendor_events = iwl_mvm_vendor_events;
	mvm->hw->wiphy->n_vendor_events = ARRAY_SIZE(iwl_mvm_vendor_events);

	spin_lock_bh(&device_list_lock);
	list_add_tail(&mvm->list, &device_list);
	spin_unlock_bh(&device_list_lock);
}

void iwl_mvm_vendor_cmds_unregister(struct iwl_mvm *mvm)
{
	spin_lock_bh(&device_list_lock);
	list_del(&mvm->list);
	spin_unlock_bh(&device_list_lock);
}

static void
iwl_mvm_send_csi_event(struct iwl_mvm *mvm,
		       void *hdr, unsigned int hdr_len,
		       void **data, unsigned int *len)
{
	unsigned int data_len = 0;
	struct sk_buff *msg;
	struct nlattr *dattr;
	u8 *pos;
	int i;

	if (!mvm->csi_portid)
		return;

	for (i = 0; len[i] && data[i]; i++)
		data_len += len[i];

	msg = cfg80211_vendor_event_alloc_ucast(mvm->hw->wiphy, NULL,
						mvm->csi_portid,
						100 + hdr_len + data_len,
						IWL_MVM_VENDOR_EVENT_IDX_CSI,
						GFP_KERNEL);

	if (!msg)
		return;

	if (nla_put(msg, IWL_MVM_VENDOR_ATTR_CSI_HDR, hdr_len, hdr))
		goto nla_put_failure;

	dattr = nla_reserve(msg, IWL_MVM_VENDOR_ATTR_CSI_DATA, data_len);
	if (!dattr)
		goto nla_put_failure;

	pos = nla_data(dattr);
	for (i = 0; len[i] && data[i]; i++) {
		memcpy(pos, data[i], len[i]);
		pos += len[i];
	}

	cfg80211_vendor_event(msg, GFP_KERNEL);
	return;

 nla_put_failure:
	kfree_skb(msg);
}

static inline u64 iwl_mvm_get_64_bit(u32 high, u32 low)
{
	return ((u64)high << 32) | low;
}

void iwl_mvm_time_sync_msmt_confirm_event(struct iwl_mvm *mvm,
					  struct iwl_rx_cmd_buffer *rxb)
{
	struct iwl_rx_packet *pkt = rxb_addr(rxb);
	struct iwl_time_msmt_cfm_notify *cfm_notify = (void *)pkt->data;
	u64 t1;
	u64 t4;

	struct sk_buff *msg =
		cfg80211_vendor_event_alloc(mvm->hw->wiphy, mvm->time_sync_wdev,
					    200,
					    IWL_MVM_VENDOR_EVENT_IDX_TSM_CFM,
					    GFP_ATOMIC);
	if (!msg)
		return;

	t1 = iwl_mvm_get_64_bit(le32_to_cpu(cfm_notify->t1_hi),
				le32_to_cpu(cfm_notify->t1_lo));
	t4 = iwl_mvm_get_64_bit(le32_to_cpu(cfm_notify->t4_hi),
				le32_to_cpu(cfm_notify->t4_lo));

	if (!t1 || !t4)
		IWL_WARN(mvm, "TSM CFM: Rx'ed zero timestamp(s), t1:%llu, t4:%llu\n",
			 t1, t4);

	if (nla_put(msg, IWL_MVM_VENDOR_ATTR_ADDR,
		    ETH_ALEN, cfm_notify->peer_addr) ||
		nla_put_u32(msg, IWL_MVM_VENDOR_ATTR_TIME_SYNC_DIALOG_TOKEN,
			    le32_to_cpu(cfm_notify->dialog_token)) ||
		nla_put_u64_64bit(msg, IWL_MVM_VENDOR_ATTR_TIME_SYNC_T1, t1,
				  IWL_MVM_VENDOR_ATTR_PAD) ||
		nla_put_u32(msg, IWL_MVM_VENDOR_ATTR_TIME_SYNC_T1_MAX_ERROR,
			    le32_to_cpu(cfm_notify->t1_max_err)) ||
		nla_put_u64_64bit(msg, IWL_MVM_VENDOR_ATTR_TIME_SYNC_T4, t4,
				  IWL_MVM_VENDOR_ATTR_PAD) ||
		nla_put_u32(msg, IWL_MVM_VENDOR_ATTR_TIME_SYNC_T4_MAX_ERROR,
			    le32_to_cpu(cfm_notify->t4_max_err))) {
		goto nla_put_failure;
	}

	cfg80211_vendor_event(msg, GFP_ATOMIC);
	return;

 nla_put_failure:
	kfree_skb(msg);
}

void iwl_mvm_time_sync_msmt_event(struct iwl_mvm *mvm,
				  struct iwl_rx_cmd_buffer *rxb)
{
	struct iwl_rx_packet *pkt = rxb_addr(rxb);
	struct iwl_time_msmt_notify *msmt_notify = (void *)pkt->data;
	u64 t1;
	u64 t4;
	u64 t2;
	u64 t3;

	struct sk_buff *msg =
		cfg80211_vendor_event_alloc(mvm->hw->wiphy, mvm->time_sync_wdev,
					    200 + PTP_CTX_MAX_DATA_SIZE,
					    IWL_MVM_VENDOR_EVENT_IDX_TSM_MSMT,
					    GFP_ATOMIC);
	if (!msg)
		return;

	t1 = iwl_mvm_get_64_bit(le32_to_cpu(msmt_notify->t1_hi),
				le32_to_cpu(msmt_notify->t1_lo));
	t4 = iwl_mvm_get_64_bit(le32_to_cpu(msmt_notify->t4_hi),
				le32_to_cpu(msmt_notify->t4_lo));
	t2 = iwl_mvm_get_64_bit(le32_to_cpu(msmt_notify->t2_hi),
				le32_to_cpu(msmt_notify->t2_lo));
	t3 = iwl_mvm_get_64_bit(le32_to_cpu(msmt_notify->t3_hi),
				le32_to_cpu(msmt_notify->t3_lo));

	if (!t1 || !t4)
		IWL_WARN(mvm, "TSM MSMT: Rx'ed zero timestamps, t1:%llu, t4:%llu\n",
			 t1, t4);

	if (!t2 || !t3)
		IWL_WARN(mvm, "TSM MSMT: Rx'ed zero timestamps, t2:%llu, t3:%llu\n",
			 t2, t3);

	if (nla_put(msg, IWL_MVM_VENDOR_ATTR_ADDR,
		    ETH_ALEN, msmt_notify->peer_addr) ||
		nla_put_u32(msg, IWL_MVM_VENDOR_ATTR_TIME_SYNC_DIALOG_TOKEN,
			    le32_to_cpu(msmt_notify->dialog_token)) ||
		nla_put_u32(msg, IWL_MVM_VENDOR_ATTR_TIME_SYNC_FUP_DIALOG_TOKEN,
			    le32_to_cpu(msmt_notify->followup_dialog_token)) ||
		nla_put_u64_64bit(msg, IWL_MVM_VENDOR_ATTR_TIME_SYNC_T1, t1,
				  IWL_MVM_VENDOR_ATTR_PAD) ||
		nla_put_u32(msg, IWL_MVM_VENDOR_ATTR_TIME_SYNC_T1_MAX_ERROR,
			    le32_to_cpu(msmt_notify->t1_max_err)) ||
		nla_put_u64_64bit(msg, IWL_MVM_VENDOR_ATTR_TIME_SYNC_T4, t4,
				  IWL_MVM_VENDOR_ATTR_PAD) ||
		nla_put_u32(msg, IWL_MVM_VENDOR_ATTR_TIME_SYNC_T4_MAX_ERROR,
			    le32_to_cpu(msmt_notify->t4_max_err)) ||
		nla_put_u64_64bit(msg, IWL_MVM_VENDOR_ATTR_TIME_SYNC_T2, t2,
				  IWL_MVM_VENDOR_ATTR_PAD) ||
		nla_put_u32(msg, IWL_MVM_VENDOR_ATTR_TIME_SYNC_T2_MAX_ERROR,
			    le32_to_cpu(msmt_notify->t2_max_err)) ||
		nla_put_u64_64bit(msg, IWL_MVM_VENDOR_ATTR_TIME_SYNC_T3, t3,
				  IWL_MVM_VENDOR_ATTR_PAD) ||
		nla_put_u32(msg, IWL_MVM_VENDOR_ATTR_TIME_SYNC_T3_MAX_ERROR,
			    le32_to_cpu(msmt_notify->t3_max_err))) {
		goto nla_put_failure;
	}

	if (mvm->time_msmt_cfg == IWL_MVM_VENDOR_TIME_SYNC_PROTOCOL_FTM) {
		if (nla_put(msg, IWL_MVM_VENDOR_ATTR_TIME_SYNC_VS_DATA,
			    msmt_notify->ptp.ftm.length, msmt_notify->ptp.ftm.data))
			goto nla_put_failure;
	} else if (mvm->time_msmt_cfg == IWL_MVM_VENDOR_TIME_SYNC_PROTOCOL_TM) {
		if (nla_put(msg, IWL_MVM_VENDOR_ATTR_TIME_SYNC_VS_DATA,
			    msmt_notify->ptp.tm.length, msmt_notify->ptp.tm.data))
			goto nla_put_failure;
	} else {
		IWL_WARN(mvm, "TSM MSMT: Unknown protocol config saved %d\n",
			 mvm->time_msmt_cfg);
		goto nla_put_failure;
	}

	cfg80211_vendor_event(msg, GFP_ATOMIC);
	return;

nla_put_failure:
	IWL_ERR(mvm, "(%d) TSM MSMT: nla_put failed\n", __LINE__);
	kfree_skb(msg);
}

static void iwl_mvm_csi_free_pages(struct iwl_mvm *mvm)
{
	int idx;

	for (idx = 0; idx < ARRAY_SIZE(mvm->csi_data_entries); idx++) {
		if (mvm->csi_data_entries[idx].page) {
			__free_pages(mvm->csi_data_entries[idx].page,
				     mvm->csi_data_entries[idx].page_order);
			memset(&mvm->csi_data_entries[idx], 0,
			       sizeof(mvm->csi_data_entries[idx]));
		}
	}
}

static void iwl_mvm_csi_steal(struct iwl_mvm *mvm, unsigned int idx,
			      struct iwl_rx_cmd_buffer *rxb)
{
	/* firmware is ... confused */
	if (WARN_ON(mvm->csi_data_entries[idx].page)) {
		iwl_mvm_csi_free_pages(mvm);
		return;
	}

	mvm->csi_data_entries[idx].page = rxb_steal_page(rxb);
	mvm->csi_data_entries[idx].page_order = rxb->_rx_page_order;
	mvm->csi_data_entries[idx].offset = rxb->_offset;
}

void iwl_mvm_rx_csi_header(struct iwl_mvm *mvm, struct iwl_rx_cmd_buffer *rxb)
{
	iwl_mvm_csi_steal(mvm, 0, rxb);
}

static void iwl_mvm_csi_complete(struct iwl_mvm *mvm)
{
	struct iwl_rx_packet *hdr_pkt;
	struct iwl_csi_data_buffer *hdr_buf = &mvm->csi_data_entries[0];
	void *data[IWL_CSI_MAX_EXPECTED_CHUNKS + 1] = {};
	unsigned int len[IWL_CSI_MAX_EXPECTED_CHUNKS + 1] = {};
	unsigned int csi_hdr_len;
	void *csi_hdr;
	int i;

	/*
	 * Ensure we have the right # of entries, the local data/len
	 * variables include a terminating entry, csi_data_entries
	 * instead has one place for the header.
	 */
	BUILD_BUG_ON(ARRAY_SIZE(data) < ARRAY_SIZE(mvm->csi_data_entries));
	BUILD_BUG_ON(ARRAY_SIZE(len) < ARRAY_SIZE(mvm->csi_data_entries));

	/* need at least the header and first fragment */
	if (WARN_ON(!hdr_buf->page || !mvm->csi_data_entries[1].page)) {
		iwl_mvm_csi_free_pages(mvm);
		return;
	}

	hdr_pkt = (void *)((unsigned long)page_address(hdr_buf->page) +
			   hdr_buf->offset);
	csi_hdr = (void *)hdr_pkt->data;
	csi_hdr_len = iwl_rx_packet_payload_len(hdr_pkt);

	for (i = 1; i < ARRAY_SIZE(mvm->csi_data_entries); i++) {
		struct iwl_csi_data_buffer *buf = &mvm->csi_data_entries[i];
		struct iwl_rx_packet *pkt;
		struct iwl_csi_chunk_notification *chunk;
		unsigned int chunk_len;

		if (!buf->page)
			break;

		pkt = (void *)((unsigned long)page_address(buf->page) +
			       buf->offset);
		chunk = (void *)pkt->data;
		chunk_len = iwl_rx_packet_payload_len(pkt);

		if (sizeof(*chunk) + le32_to_cpu(chunk->size) > chunk_len)
			goto free;

		data[i - 1] = chunk->data;
		len[i - 1] = le32_to_cpu(chunk->size);
	}

	iwl_mvm_send_csi_event(mvm, csi_hdr, csi_hdr_len, data, len);

 free:
	iwl_mvm_csi_free_pages(mvm);
}

void iwl_mvm_rx_csi_chunk(struct iwl_mvm *mvm, struct iwl_rx_cmd_buffer *rxb)
{
	struct iwl_rx_packet *pkt = rxb_addr(rxb);
	struct iwl_csi_chunk_notification *chunk = (void *)pkt->data;
	int num;
	int idx;

	switch (mvm->cmd_ver.csi_notif) {
	case 1:
		num = le16_get_bits(chunk->ctl,
				    IWL_CSI_CHUNK_CTL_NUM_MASK_VER_1);
		idx = le16_get_bits(chunk->ctl,
				    IWL_CSI_CHUNK_CTL_IDX_MASK_VER_1) + 1;
		break;
	case 2:
		num = le16_get_bits(chunk->ctl,
				    IWL_CSI_CHUNK_CTL_NUM_MASK_VER_2);
		idx = le16_get_bits(chunk->ctl,
				    IWL_CSI_CHUNK_CTL_IDX_MASK_VER_2) + 1;
		break;
	default:
		WARN_ON(1);
		return;
	}

	/* -1 to account for the header we also store there */
	if (WARN_ON_ONCE(idx >= ARRAY_SIZE(mvm->csi_data_entries) - 1))
		return;

	iwl_mvm_csi_steal(mvm, idx, rxb);

	if (num == idx)
		iwl_mvm_csi_complete(mvm);
}

void iwl_mvm_send_roaming_forbidden_event(struct iwl_mvm *mvm,
					  struct ieee80211_vif *vif,
					  bool forbidden)
{
	struct sk_buff *msg =
		cfg80211_vendor_event_alloc(mvm->hw->wiphy,
					    ieee80211_vif_to_wdev(vif),
					    200, IWL_MVM_VENDOR_EVENT_IDX_ROAMING_FORBIDDEN,
					    GFP_ATOMIC);
	if (!msg)
		return;

	if (WARN_ON(!vif))
		return;

	if (nla_put(msg, IWL_MVM_VENDOR_ATTR_VIF_ADDR,
		    ETH_ALEN, vif->addr) ||
	    nla_put_u8(msg, IWL_MVM_VENDOR_ATTR_ROAMING_FORBIDDEN, forbidden))
		goto nla_put_failure;

	cfg80211_vendor_event(msg, GFP_ATOMIC);
	return;

 nla_put_failure:
	kfree_skb(msg);
}
