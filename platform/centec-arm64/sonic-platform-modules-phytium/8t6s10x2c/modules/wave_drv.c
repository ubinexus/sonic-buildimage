#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/gpio/consumer.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/of_device.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>


#define PINMUX_MASK        (0x3 << 12)
#define PINMUX_VALUE       (0x1 << 12)

struct wave_gpio_device_data {
    struct gpio_desc *gpio;
    struct hrtimer timer;
    ktime_t period;
    bool gpio_state;
};

static enum hrtimer_restart timer_callback(struct hrtimer *timer)
{
    struct wave_gpio_device_data *data = container_of(timer, struct wave_gpio_device_data, timer);
    
    data->gpio_state = !data->gpio_state;
    gpiod_set_value(data->gpio, data->gpio_state);
    
    //reset timer
    hrtimer_forward_now(timer, data->period);
    return HRTIMER_RESTART;
}

static resource_size_t phy_addr;  // 保存物理地址
static resource_size_t region_size; // 保存映射大小


static int wave_gpio_probe(struct platform_device *pdev)
{
    struct wave_gpio_device_data *data;
    int ret;
    
    data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
    if (!data)
        return -ENOMEM;

    struct resource *res = platform_get_resource(pdev, IORESOURCE_MEM, 0);  
    if (!res) {  
        dev_err(&pdev->dev, "Failed to get MEM resource\n");  
        return -ENODEV;  
    }

    phy_addr = res->start;
    region_size = resource_size(res);  // 计算大小 = res->end - res->start + 1

    // 打印物理地址和大小（16进制和10进制）
    printk("Physical address: 0x%llx (size: 0x%llx, %lld bytes)\n",
             (u64)phy_addr, (u64)region_size, (u64)region_size);
    
    void __iomem *reg_base = devm_ioremap_resource(&pdev->dev, res);  
    if (IS_ERR(reg_base)) {  
        return PTR_ERR(reg_base);  
    }
    u32 reg_val = readl(reg_base);
    reg_val &= ~PINMUX_MASK;
    reg_val |= PINMUX_VALUE;
    writel(reg_val, reg_base);

    data->gpio = devm_gpiod_get(&pdev->dev, NULL, GPIOD_OUT_LOW);
    if (IS_ERR(data->gpio)) {
        dev_err(&pdev->dev, "Failed to get GPIO\n");
        return PTR_ERR(data->gpio);
    }

    data->period = ktime_set(0, 500000000); // 500ms in nanoseconds
    data->gpio_state = false;
    hrtimer_init(&data->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    data->timer.function = timer_callback;
    
    hrtimer_start(&data->timer, data->period, HRTIMER_MODE_REL);
    
    platform_set_drvdata(pdev, data);
    dev_info(&pdev->dev, "Device probed successfully\n");
    gpiod_set_value(data->gpio, 1);
    return 0;
}

static int wave_gpio_remove(struct platform_device *pdev)
{
    struct wave_gpio_device_data *data = platform_get_drvdata(pdev);
    hrtimer_cancel(&data->timer);
    gpiod_set_value(data->gpio, 0);
    dev_info(&pdev->dev, "Wave GPIO driver removed\n");
    return 0;
}


static const struct of_device_id wave_gpio_match[] = {
    { .compatible = "wave-gpio-device" },
    { },
};
MODULE_DEVICE_TABLE(of, wave_gpio_match);

static struct platform_driver wave_gpio_driver = {
    .driver = {
        .name = "wave-gpio-driver",
        .of_match_table = wave_gpio_match,
    },
    .probe = wave_gpio_probe,
    .remove = wave_gpio_remove,
};

module_platform_driver(wave_gpio_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("xylx");
MODULE_DESCRIPTION("Driver for wave gpio device");
