/*
 * drivers/net/phy/realtek.c
 *
 * Driver for Realtek PHYs
 *
 * Author: Johnson Leung <r58129@freescale.com>
 *
 * Copyright (c) 2004 Freescale Semiconductor, Inc.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */
#include <linux/phy.h>
#include <linux/module.h>
#include <linux/of.h>

struct rtl8211f_phy_priv {
	bool eee_1000_disable;
	bool eee_100_disable;
};

#define RTL821x_PHYSR		0x11
#define RTL821x_PHYSR_DUPLEX	0x2000
#define RTL821x_PHYSR_SPEED	0xc000
#define RTL821x_INER		0x12
#define RTL821x_INER_INIT	0x6400
#define RTL821x_INSR		0x13
#define RTL8211E_INER_LINK_STATUS 0x400

#define RTL8211F_INER_LINK_STATUS 0x0010
#define RTL8211F_INSR		0x1d
#define RTL8211F_PAGE_SELECT	0x1f
#define RTL8211F_TX_DELAY	0x100

MODULE_DESCRIPTION("Realtek PHY driver");
MODULE_AUTHOR("Johnson Leung");
MODULE_LICENSE("GPL");

static int rtl821x_ack_interrupt(struct phy_device *phydev)
{
	int err;

	err = phy_read(phydev, RTL821x_INSR);

	return (err < 0) ? err : 0;
}

static int rtl8211f_ack_interrupt(struct phy_device *phydev)
{
	int err;

	phy_write(phydev, RTL8211F_PAGE_SELECT, 0xa43);
	err = phy_read(phydev, RTL8211F_INSR);
	/* restore to default page 0 */
	phy_write(phydev, RTL8211F_PAGE_SELECT, 0x0);

	return (err < 0) ? err : 0;
}

static int rtl8211b_config_intr(struct phy_device *phydev)
{
	int err;

	if (phydev->interrupts == PHY_INTERRUPT_ENABLED)
		err = phy_write(phydev, RTL821x_INER,
				RTL821x_INER_INIT);
	else
		err = phy_write(phydev, RTL821x_INER, 0);

	return err;
}

static int rtl8211e_config_intr(struct phy_device *phydev)
{
	int err;

	if (phydev->interrupts == PHY_INTERRUPT_ENABLED)
		err = phy_write(phydev, RTL821x_INER,
				RTL8211E_INER_LINK_STATUS);
	else
		err = phy_write(phydev, RTL821x_INER, 0);

	return err;
}

static int rtl8211f_config_intr(struct phy_device *phydev)
{
	int err;

	if (phydev->interrupts == PHY_INTERRUPT_ENABLED)
		err = phy_write(phydev, RTL821x_INER,
				RTL8211F_INER_LINK_STATUS);
	else
		err = phy_write(phydev, RTL821x_INER, 0);

	return err;
}

static void rtl8211f_force_eee(struct phy_device *phydev)
{
	struct rtl8211f_phy_priv *priv = phydev->priv;
	u16 val;

	if (priv->eee_1000_disable || priv->eee_100_disable) {
		val = phy_read_mmd_indirect(phydev, MDIO_AN_EEE_ADV,
					    MDIO_MMD_AN);

		if (priv->eee_1000_disable)
			val &= ~MDIO_AN_EEE_ADV_1000T;
		if (priv->eee_100_disable)
			val &= ~MDIO_AN_EEE_ADV_100TX;

		phy_write_mmd_indirect(phydev, MDIO_AN_EEE_ADV,
				       MDIO_MMD_AN, val);
	}
}

static int rtl8211f_config_init(struct phy_device *phydev)
{
	int ret;
	u16 reg;

	ret = genphy_config_init(phydev);
	if (ret < 0)
		return ret;

	rtl8211f_force_eee(phydev);

	phy_write(phydev, RTL8211F_PAGE_SELECT, 0xd08);
	reg = phy_read(phydev, 0x11);

	/* enable TX-delay for rgmii-id and rgmii-txid, otherwise disable it */
	if (phydev->interface == PHY_INTERFACE_MODE_RGMII_ID ||
	    phydev->interface == PHY_INTERFACE_MODE_RGMII_TXID)
		reg |= RTL8211F_TX_DELAY;
	else
		reg &= ~RTL8211F_TX_DELAY;

	phy_write(phydev, 0x11, reg);
	/* restore to default page 0 */
	phy_write(phydev, RTL8211F_PAGE_SELECT, 0x0);

	return 0;
}

static int rtl8211f_phy_probe(struct phy_device *phydev)
{
	struct device *dev = &phydev->mdio.dev;
	struct device_node *of_node = dev->of_node;
	struct rtl8211f_phy_priv *priv;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	if (of_property_read_bool(of_node, "realtek,disable-eee-1000t"))
		priv->eee_1000_disable= true;
	if (of_property_read_bool(of_node, "realtek,disable-eee-100t"))
		priv->eee_100_disable= true;

	phydev->priv = priv;

	return 0;
}

static struct phy_driver realtek_drvs[] = {
	{
		.phy_id         = 0x00008201,
		.name           = "RTL8201CP Ethernet",
		.phy_id_mask    = 0x0000ffff,
		.features       = PHY_BASIC_FEATURES,
		.flags          = PHY_HAS_INTERRUPT,
		.config_aneg    = &genphy_config_aneg,
		.read_status    = &genphy_read_status,
	}, {
		.phy_id		= 0x001cc912,
		.name		= "RTL8211B Gigabit Ethernet",
		.phy_id_mask	= 0x001fffff,
		.features	= PHY_GBIT_FEATURES,
		.flags		= PHY_HAS_INTERRUPT,
		.config_aneg	= &genphy_config_aneg,
		.read_status	= &genphy_read_status,
		.ack_interrupt	= &rtl821x_ack_interrupt,
		.config_intr	= &rtl8211b_config_intr,
	}, {
		.phy_id		= 0x001cc914,
		.name		= "RTL8211DN Gigabit Ethernet",
		.phy_id_mask	= 0x001fffff,
		.features	= PHY_GBIT_FEATURES,
		.flags		= PHY_HAS_INTERRUPT,
		.config_aneg	= genphy_config_aneg,
		.read_status	= genphy_read_status,
		.ack_interrupt	= rtl821x_ack_interrupt,
		.config_intr	= rtl8211e_config_intr,
		.suspend	= genphy_suspend,
		.resume		= genphy_resume,
	}, {
		.phy_id		= 0x001cc915,
		.name		= "RTL8211E Gigabit Ethernet",
		.phy_id_mask	= 0x001fffff,
		.features	= PHY_GBIT_FEATURES,
		.flags		= PHY_HAS_INTERRUPT,
		.config_aneg	= &genphy_config_aneg,
		.read_status	= &genphy_read_status,
		.ack_interrupt	= &rtl821x_ack_interrupt,
		.config_intr	= &rtl8211e_config_intr,
		.suspend	= genphy_suspend,
		.resume		= genphy_resume,
	}, {
		.phy_id		= 0x001cc916,
		.name		= "RTL8211F Gigabit Ethernet",
		.phy_id_mask	= 0x001fffff,
		.features	= PHY_GBIT_FEATURES,
		.flags		= PHY_HAS_INTERRUPT,
		.probe		= &rtl8211f_phy_probe,
		.config_aneg	= &genphy_config_aneg,
		.config_init	= &rtl8211f_config_init,
		.read_status	= &genphy_read_status,
		.ack_interrupt	= &rtl8211f_ack_interrupt,
		.config_intr	= &rtl8211f_config_intr,
		.suspend	= genphy_suspend,
		.resume		= genphy_resume,
	},
};

module_phy_driver(realtek_drvs);

static struct mdio_device_id __maybe_unused realtek_tbl[] = {
	{ 0x001cc912, 0x001fffff },
	{ 0x001cc914, 0x001fffff },
	{ 0x001cc915, 0x001fffff },
	{ 0x001cc916, 0x001fffff },
	{ }
};

MODULE_DEVICE_TABLE(mdio, realtek_tbl);
