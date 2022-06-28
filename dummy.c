#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/fs.h>
#include <linux/fcntl.h>
#include <linux/ctype.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/pagemap.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/kthread.h>
#include <linux/timer.h>
#include <linux/sched.h>
#include <linux/ktime.h>
#include <linux/jiffies.h>
#include <linux/semaphore.h>
#include <linux/ioctl.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>

#define START 1
#define END 100
#define INTERVAL1 jiffies + HZ
#define INTERVAL2 jiffies + 2 * HZ

struct timer_list timer1, timer2;

struct task_struct *kth1, *kth2;

struct semaphore sem1, sem2;

int v1, v2;

static void _timer1(void) {
	printk("COUNT1: %d\n", v1);
	up(&sem1);
}

static void _timer2(void) {
	printk("COUNT2: %d\n", v2);
	up(&sem2);
}

static void start_timer1(void) {
	timer_setup(&timer1, (void *)_timer1, 0);
	timer1.expires = INTERVAL1;
	add_timer(&timer1);
}

static void start_timer2(void) {
	timer_setup(&timer2, (void *)_timer2, 0);
	timer2.expires = INTERVAL2;
	add_timer(&timer2);
}

static void _kth1(void) {
	down(&sem1);
	for(v1 = START + 1; v1 <= END; v1 += 1) {
		mod_timer(&timer1, INTERVAL1);
		down(&sem1);
	}
}

static void _kth2(void) {
	down(&sem2);
	for(v2 = START + 1; v2 <= END; v2 += 1) {
		mod_timer(&timer2, INTERVAL2);
		down(&sem2);
	}
}

//open
static int dummy_open(struct inode *inodp, struct file *filp) {
	v1 = START;
	v2 = START;

	sema_init(&sem1, 0);
	sema_init(&sem2, 0);

	kth1 = kthread_create((void *)_kth1, NULL, "kthread1");
	wake_up_process(kth1);
	
	kth2 = kthread_create((void *)_kth2, NULL, "kthread2");
	wake_up_process(kth2);
	
	return 0;
}

static int dummy_release(struct inode *inodp, struct file *filp) {
	del_timer(&timer1);
	del_timer(&timer2);
	
	kthread_stop(kth1);
	kthread_stop(kth2);
	
	return 0;
}

static long dummy_ioctl(struct file *filp, unsigned int cmd, unsigned long arg) {
	switch(cmd) {
		case 10:                        
			del_timer(&timer1);
			return 0;
			break;
		case 11:                       
			if(timer_pending(&timer1)) return -1;
			else { start_timer1(); return 0; }
			break;
		case 12:                       
			del_timer(&timer1);
			v1 = START;
			return 0;
			break;
		case 20:                       
			del_timer(&timer2);
			return 0;
			break;
		case 21:                        
			if(timer_pending(&timer2)) return -1;
			else { start_timer2(); return 0; }
			break;
		case 22:                        
			del_timer(&timer2);
			v2 = START;
			return 0;
			break;
		case 30:                        
			del_timer(&timer1);
			del_timer(&timer2);
			return 0;
			break;
		case 31:                        
			if(timer_pending(&timer1) && timer_pending(&timer2)) return -1;
			if(timer_pending(&timer1) && !timer_pending(&timer2)) { start_timer2(); return 2; }
			if(!timer_pending(&timer1) && timer_pending(&timer2)) { start_timer1(); return 1; }
			if(!timer_pending(&timer1) && !timer_pending(&timer2)) { start_timer1(); start_timer2(); return 0; }
			break;
		case 32:                        //reset thread1 and thread2
			del_timer(&timer1);
			del_timer(&timer2);
			v1 = START;
			v2 = START;
			return 0;
			break;
	}
	return 0;
}

static struct file_operations dummy_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = dummy_ioctl,
	.open = dummy_open,
	.release = dummy_release
};

static struct miscdevice dummy_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "dummy",
	.nodename = "dummy",
	.fops = &dummy_fops,
	.mode = 0666,
};
static int __init dummy_register(void)
{
	int err;
	err = misc_register(&dummy_dev);
	if (err == 0) {
		printk("dummy dev registered, minor %d\n", dummy_dev.minor);
	}
	return err;
}
static void __exit dummy_unregister(void)
{
	misc_deregister(&dummy_dev);
}

static int __init dummy_init(void)
{
	return dummy_register();
}
static void __exit dummy_exit(void)
{
	dummy_unregister();
}

module_init(dummy_init);
module_exit(dummy_exit);

MODULE_AUTHOR("MRZC");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("TWO THREADS");
