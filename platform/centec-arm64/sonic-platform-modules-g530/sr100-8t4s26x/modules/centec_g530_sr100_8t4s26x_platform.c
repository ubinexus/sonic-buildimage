#include <linux/init.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/platform_data/pca954x.h>
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

#if SEP("defines")

#define CTC_GPIO_BASE         496
int xirq_gpio_0 = 0;
int xirq_gpio_1 = 0;
int xirq_gpio_15 = 0;
#define IS_INVALID_PTR(_PTR_) ((_PTR_ == NULL) || IS_ERR(_PTR_))
#define IS_VALID_PTR(_PTR_) (!IS_INVALID_PTR(_PTR_))
#endif

#if SEP("ctc:pinctl")
u8 ctc_gpio_set(u8 gpio_pin, u8 val)
{
    gpio_set_value_cansleep(gpio_pin + CTC_GPIO_BASE, val);
    return 0;
}

u8 ctc_gpio_get(u8 gpio_pin)
{
    return gpio_get_value_cansleep(gpio_pin + CTC_GPIO_BASE);
}

u8 ctc_gpio_direction_config(u8 gpio_pin, u8 dir,u8 default_out)
{
    return dir ? gpio_direction_input(gpio_pin + CTC_GPIO_BASE)
               : gpio_direction_output(gpio_pin + CTC_GPIO_BASE,default_out);
}

static void ctc_pincrtl_init(void)
{
    /* configure mgmt-phy reset-pin output on product, mgmt-phy release must before this */
    ctc_gpio_direction_config(4, 0, 1);
    /* configure power-up pin output on product */
    ctc_gpio_direction_config(6, 0, 0);
    /* configure phy interrupt pin input */
    ctc_gpio_direction_config(0, 1, 0);
    ctc_gpio_direction_config(1, 1, 0);
    /* configure phy reset-pin output, for release phy */
    ctc_gpio_direction_config(5, 0, 1);

    return;
}

static void ctc_irq_init(void)
{
    struct device_node *xnp;
    for_each_node_by_type(xnp, "ctc-irq")
    {
        if (of_device_is_compatible(xnp, "centec,ctc-irq"))
        {
            xirq_gpio_0 = irq_of_parse_and_map(xnp, 0);
            printk(KERN_INFO "ctc-irq GPIO0 IRQ is %d\n", xirq_gpio_0);
            xirq_gpio_1 = irq_of_parse_and_map(xnp, 1);
            printk(KERN_INFO "ctc-irq GPIO1 IRQ is %d\n", xirq_gpio_1);
            xirq_gpio_15 = irq_of_parse_and_map(xnp, 2);
            printk(KERN_INFO "ctc-irq GPIO15 IRQ is %d\n", xirq_gpio_15);
        }
    }
    return;
}
#endif

#if SEP("i2c:smbus")
static int g530_sr100_8t4s26x_smbus_read_reg(struct i2c_client *client, unsigned char reg, unsigned char* value)
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

static int g530_sr100_8t4s26x_smbus_write_reg(struct i2c_client *client, unsigned char reg, unsigned char value)
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
static struct i2c_adapter *i2c_adp_master          = NULL; /* i2c-0-cpu */

static int g530_sr100_8t4s26x_init_i2c_master(void)
{
    /* find i2c-core master */
    i2c_adp_master = i2c_get_adapter(0);
    if(IS_INVALID_PTR(i2c_adp_master))
    {
        i2c_adp_master = NULL;
        printk(KERN_CRIT "g530_sr100_8t4s26x_init_i2c_master can't find i2c-core bus\n");
        return -1;
    }
    
    return 0;
}

static int g530_sr100_8t4s26x_exit_i2c_master(void)
{
    /* uninstall i2c-core master */
    if(IS_VALID_PTR(i2c_adp_master)) {
        i2c_put_adapter(i2c_adp_master);
        i2c_adp_master = NULL;
    }
    
    return 0;
}
#endif

#if SEP("i2c:gpio")
#define PCA9535_BANK_NUM 2

#define PCA9535_INPUT_PORT_REG_BANK0 0x0
#define PCA9535_INPUT_PORT_REG_BANK1 0x1

#define PCA9535_OUTPUT_PORT_REG_BANK0 0x2
#define PCA9535_OUTPUT_PORT_REG_BANK1 0x3

#define PCA9535_POLARITY_INVE_REG_BANK0 0x4
#define PCA9535_POLARITY_INVE_REG_BANK1 0x5

#define PCA9535_DIR_CTRL_REG_BANK0 0x6
#define PCA9535_DIR_CTRL_REG_BANK1 0x7

static struct i2c_adapter *i2c_adp_gpio0           = NULL; /* gpio0 */
static struct i2c_board_info i2c_dev_gpio0 = {
    I2C_BOARD_INFO("i2c-gpio0", 0x21),
};
static struct i2c_client  *i2c_client_gpio0      = NULL;

static int g530_sr100_8t4s26x_init_i2c_gpio(void)
{
    int ret = 0;

    if (IS_INVALID_PTR(i2c_adp_master))
    {
         printk(KERN_CRIT "g530_sr100_8t4s26x_init_i2c_gpio can't find i2c-core bus\n");
         return -1;
    }

    i2c_adp_gpio0 = i2c_get_adapter(0);
    if(IS_INVALID_PTR(i2c_adp_gpio0))
    {
        i2c_adp_gpio0 = NULL;
        printk(KERN_CRIT "get g530_sr100_8t4s26x gpio0 i2c-adp failed\n");
        return -1;
    }

    i2c_client_gpio0 = i2c_new_device(i2c_adp_gpio0, &i2c_dev_gpio0);
    if(IS_INVALID_PTR(i2c_client_gpio0))
    {
        i2c_client_gpio0 = NULL;
        printk(KERN_CRIT "create g530_sr100_8t4s26x board i2c client gpio0 failed\n");
        return -1;
    }
    /* set sfp tx enable and light off location led */
    ret  = g530_sr100_8t4s26x_smbus_write_reg(i2c_client_gpio0, PCA9535_OUTPUT_PORT_REG_BANK0, 0xff);
    /* set sfp rx enable and light on green sys led */
    ret  += g530_sr100_8t4s26x_smbus_write_reg(i2c_client_gpio0, PCA9535_OUTPUT_PORT_REG_BANK1, 0xbf);
    /*config dir*/
    ret  += g530_sr100_8t4s26x_smbus_write_reg(i2c_client_gpio0, PCA9535_DIR_CTRL_REG_BANK0, 0x80);
    ret  += g530_sr100_8t4s26x_smbus_write_reg(i2c_client_gpio0, PCA9535_DIR_CTRL_REG_BANK1, 0x0);

    return ret;
}

static int g530_sr100_8t4s26x_exit_i2c_gpio(void)
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

#if SEP("drivers:leds")
extern void g530_sr100_8t4s26x_led_set(struct led_classdev *led_cdev, enum led_brightness set_value);
extern enum led_brightness g530_sr100_8t4s26x_led_get(struct led_classdev *led_cdev);

static struct led_classdev led_dev_system = {
    .name = "system",
    .brightness_set = g530_sr100_8t4s26x_led_set,
    .brightness_get = g530_sr100_8t4s26x_led_get,
};

static struct led_classdev led_dev_alarm = {
    .name = "alarm",
    .brightness_set = g530_sr100_8t4s26x_led_set,
    .brightness_get = g530_sr100_8t4s26x_led_get,
};

static struct led_classdev led_dev_idn = {
    .name = "idn",
    .brightness_set = g530_sr100_8t4s26x_led_set,
    .brightness_get = g530_sr100_8t4s26x_led_get,
};

void g530_sr100_8t4s26x_led_set(struct led_classdev *led_cdev, enum led_brightness set_value)
{
    int ret = 0;
    unsigned char reg = 0;
    unsigned char mask = 0;
    unsigned char shift = 0;
    unsigned char led_value = 0;
    struct i2c_client *i2c_led_client = i2c_client_gpio0;

    if (set_value != LED_OFF && set_value != LED_ON)
    {
        printk(KERN_CRIT "Error: write %s led value %d failed\n", led_cdev->name, set_value);
        return;
    }

    if (0 == strcmp(led_dev_system.name, led_cdev->name))
    {
        reg = PCA9535_OUTPUT_PORT_REG_BANK1;
        mask = 0xbf;
        shift = 6;
    }
    else if (0 == strcmp(led_dev_alarm.name, led_cdev->name))
    {
        reg = PCA9535_OUTPUT_PORT_REG_BANK1;
        mask = 0x7f;
        shift = 7;
    }
    else if (0 == strcmp(led_dev_idn.name, led_cdev->name))
    {
        reg = PCA9535_OUTPUT_PORT_REG_BANK0;
        mask = 0xbf;
        shift = 6;
    }
    else
    {
        goto not_support;
    }

    ret = g530_sr100_8t4s26x_smbus_read_reg(i2c_led_client, reg, &led_value);
    if (ret != 0)
    {
        printk(KERN_CRIT "Error: read %s led attr failed\n", led_cdev->name);
        return;
    }
    if (set_value == LED_ON)
    {
        /*Turn on LED*/
        led_value = led_value & mask;
    }
    else 
    {
        /*Turn off LED */
        led_value = (led_value & mask) | (0x1 << shift);
    }
        
    ret = g530_sr100_8t4s26x_smbus_write_reg(i2c_led_client, reg, led_value);
    if (ret != 0)
    {
        printk(KERN_CRIT "Error: write %s led attr failed\n", led_cdev->name);
        return;
    }

    return;
    
not_support:

    printk(KERN_INFO "Error: led not support device:%s\n", led_cdev->name);
    return;
}

enum led_brightness g530_sr100_8t4s26x_led_get(struct led_classdev *led_cdev)
{
    int ret = 0;
    unsigned char reg = 0;
    unsigned char shift = 0;
    unsigned char led_value = 0;
    struct i2c_client *i2c_led_client = i2c_client_gpio0;

    if (0 == strcmp(led_dev_system.name, led_cdev->name))
    {
        reg = PCA9535_OUTPUT_PORT_REG_BANK1;
        shift = 6;
    }
    else if (0 == strcmp(led_dev_alarm.name, led_cdev->name))
    {
        reg = PCA9535_OUTPUT_PORT_REG_BANK1;
        shift = 7;
    }
    else if (0 == strcmp(led_dev_idn.name, led_cdev->name))
    {
        reg = PCA9535_OUTPUT_PORT_REG_BANK0;
        shift = 6;
    }
    else
    {
        goto not_support;
    }

    ret = g530_sr100_8t4s26x_smbus_read_reg(i2c_led_client, reg, &led_value);
    if (ret != 0)
    {
        printk(KERN_CRIT "Error: read %s led attr failed\n", led_cdev->name);
        return;
    }

    if ((led_value >> shift) & 0x1 == 0x1)
    {
        return LED_OFF;
    }
    else 
    {
        return LED_ON;
    }

not_support:

    printk(KERN_INFO "Error: not support device:%s\n", led_cdev->name);
    return 0;
}

static int g530_sr100_8t4s26x_init_led(void)
{
    int ret = 0;

    ret = led_classdev_register(NULL, &led_dev_system);
    if (ret != 0)
    {
        printk(KERN_CRIT "create g530_sr100_8t4s26x led_dev_system device failed\n");
        return -1;
    }

    ret = led_classdev_register(NULL, &led_dev_alarm);
    if (ret != 0)
    {
        printk(KERN_CRIT "create g530_sr100_8t4s26x led_dev_alarm device failed\n");
        return -1;
    }

    ret = led_classdev_register(NULL, &led_dev_idn);
    if (ret != 0)
    {
        printk(KERN_CRIT "create g530_sr100_8t4s26x led_dev_idn device failed\n");
        return -1;
    }

    return ret;
}

static int g530_sr100_8t4s26x_exit_led(void)
{
    led_classdev_unregister(&led_dev_system);
    led_classdev_unregister(&led_dev_alarm);
    led_classdev_unregister(&led_dev_idn);

    return 0;
}
#endif

static int g530_sr100_8t4s26x_init(void)
{
    int ret = 0;
    int failed = 0;
    
    printk(KERN_ALERT "install g530_sr100_8t4s26x board dirver...\n");

    ctc_irq_init();
    ctc_pincrtl_init();
    
    ret = g530_sr100_8t4s26x_init_i2c_master();
    if (ret != 0)
    {
        failed = 1;
    }

    ret = g530_sr100_8t4s26x_init_i2c_gpio();
    if (ret != 0)
    {
        failed = 1;
    }

    ret = g530_sr100_8t4s26x_init_led();
    if (ret != 0)
    {
        failed = 1;
    }

    if (failed)
        printk(KERN_INFO "install g530_sr100_8t4s26x board driver failed\n");
    else
        printk(KERN_ALERT "install g530_sr100_8t4s26x board dirver...ok\n");
    
    return 0;
}

static void g530_sr100_8t4s26x_exit(void)
{
    printk(KERN_INFO "uninstall g530_sr100_8t4s26x board dirver...\n");
    
    g530_sr100_8t4s26x_exit_led();
    g530_sr100_8t4s26x_exit_i2c_gpio();
    g530_sr100_8t4s26x_exit_i2c_master();
}

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("shil centecNetworks, Inc");
MODULE_DESCRIPTION("g530-sr100-8t4s26x board driver");
module_init(g530_sr100_8t4s26x_init);
module_exit(g530_sr100_8t4s26x_exit);
