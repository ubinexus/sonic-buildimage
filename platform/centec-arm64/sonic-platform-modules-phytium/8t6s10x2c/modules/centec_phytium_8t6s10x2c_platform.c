#include <linux/init.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/leds.h>
#include <linux/reboot.h>
#include <linux/delay.h>
#include <linux/spinlock.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <asm/irq.h>
#include <asm/types.h>
#include <asm/io.h>
#include <linux/poll.h>
#include <linux/wait.h>
#include <linux/pci.h>
#include <linux/interrupt.h>
#include <asm/delay.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/gpio.h>

#define SEP(XXX) 1
#define IS_INVALID_PTR(_PTR_) ((_PTR_ == NULL) || IS_ERR(_PTR_))
#define IS_VALID_PTR(_PTR_) (!IS_INVALID_PTR(_PTR_))

#if SEP("i2c:smbus")
static int phytium_8t6s10x2c_smbus_read_reg(struct i2c_client *client, unsigned char reg, unsigned char* value)
{
    int ret = 0;

    if (IS_INVALID_PTR(client))
    {
        printk(KERN_CRIT "invalid i2c client");
        return -1;
    }
    
    ret = i2c_smbus_read_byte_data(client, reg);
    if (ret >= 0) {
        *value = (unsigned char)ret;
    }
    else
    {
        *value = 0;
        printk(KERN_CRIT "i2c_smbus op failed: ret=%d reg=%d\n",ret ,reg);
        return ret;
    }

    return 0;
}

static int phytium_8t6s10x2c_smbus_write_reg(struct i2c_client *client, unsigned char reg, unsigned char value)
{
    int ret = 0;
    
    if (IS_INVALID_PTR(client))
    {
        printk(KERN_CRIT "invalid i2c client");
        return -1;
    }
    
    ret = i2c_smbus_write_byte_data(client, reg, value);
    if (ret != 0)
    {
        printk(KERN_CRIT "i2c_smbus op failed: ret=%d reg=%d\n",ret ,reg);
        return ret;
    }

    return 0;
}
#endif

#if SEP("i2c:master")
static struct i2c_adapter *i2c_adp_master          = NULL; /* i2c-1-cpu */

static int phytium_8t6s10x2c_init_i2c_master(void)
{
    /* find i2c-core master */
    i2c_adp_master = i2c_get_adapter(0);
    if(IS_INVALID_PTR(i2c_adp_master))
    {
        i2c_adp_master = NULL;
        printk(KERN_CRIT "phytium_8t6s10x2c_init_i2c_master can't find i2c-core bus\n");
        return -1;
    }
    
    return 0;
}

static int phytium_8t6s10x2c_exit_i2c_master(void)
{
    /* uninstall i2c-core master */
    if(IS_VALID_PTR(i2c_adp_master)) {
        i2c_put_adapter(i2c_adp_master);
        i2c_adp_master = NULL;
    }
    
    return 0;
}
#endif

//TODO!!!
#if SEP("i2c:gpio")
static struct i2c_adapter *i2c_adp_gpio0           = NULL; /* gpio0 */
/*pca9535*/
static struct i2c_board_info i2c_dev_gpio0 = {
    I2C_BOARD_INFO("i2c-gpio0", 0x21),
};
static struct i2c_client  *i2c_client_gpio0      = NULL;

static int phytium_8t6s10x2c_init_i2c_gpio(void)
{

    if (IS_INVALID_PTR(i2c_adp_master))
    {
         printk(KERN_CRIT "phytium_8t6s10x2c_init_i2c_gpio can't find i2c-core bus\n");
         return -1;
    }

    i2c_adp_gpio0 = i2c_get_adapter(0);
    if(IS_INVALID_PTR(i2c_adp_gpio0))
    {
        i2c_adp_gpio0 = NULL;
        printk(KERN_CRIT "get phytium_8t6s10x2c gpio0 i2c-adp failed\n");
        return -1;
    }

    i2c_client_gpio0 = i2c_new_client_device(i2c_adp_gpio0, &i2c_dev_gpio0);
    if(IS_INVALID_PTR(i2c_client_gpio0))
    {
        i2c_client_gpio0 = NULL;
        printk(KERN_CRIT "create phytium_8t6s10x2c board i2c client gpio0 failed\n");
        return -1;
    }

    /* gpio0 */
    /* tx/rx enable by hw default config*/

    return 0;
}

static int phytium_8t6s10x2c_exit_i2c_gpio(void)
{
    if(IS_VALID_PTR(i2c_client_gpio0)) {
        i2c_unregister_device(i2c_client_gpio0);
        i2c_client_gpio0 = NULL;
    }

    if(IS_VALID_PTR(i2c_adp_gpio0)) 
    {
        i2c_put_adapter(i2c_adp_gpio0);
        i2c_adp_gpio0 = NULL;
    }

    return 0;
}
#endif


static int phytium_8t6s10x2c_init(void)
{
    int ret = 0;
    int failed = 0;
    
    printk(KERN_ALERT "install phytium_8t6s10x2c board dirver...\n");

    ret = phytium_8t6s10x2c_init_i2c_master();
    if (ret != 0)
    {
        failed = 1;
    }

    ret = phytium_8t6s10x2c_init_i2c_gpio();
    if (ret != 0)
    {
        failed = 1;
    }

    if (failed)
        printk(KERN_INFO "install phytium_8t6s10x2c board driver failed\n");
    else
        printk(KERN_ALERT "install phytium_8t6s10x2c board dirver...ok\n");
    
    return 0;
}

static void phytium_8t6s10x2c_exit(void)
{
    printk(KERN_INFO "uninstall phytium_8t6s10x2c board dirver...\n");
    
    phytium_8t6s10x2c_exit_i2c_gpio();
    phytium_8t6s10x2c_exit_i2c_master();
}

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("shil centecNetworks, Inc");
MODULE_DESCRIPTION("phytium_8t6s10x2c_ board driver");
module_init(phytium_8t6s10x2c_init);
module_exit(phytium_8t6s10x2c_exit);
