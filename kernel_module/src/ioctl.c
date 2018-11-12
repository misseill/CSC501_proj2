// Project 2: Swastik Mittal, Smittal6; Erika Eill, Eleill
//////////////////////////////////////////////////////////////////////
//                      North Carolina State University
//
//
//
//                             Copyright 2018
//
////////////////////////////////////////////////////////////////////////
//
// This program is free software; you can redistribute it and/or modify it
// under the terms and conditions of the GNU General Public License,
// version 2, as published by the Free Software Foundation.
//
// This program is distributed in the hope it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
// more details.
//
// You should have received a copy of the GNU General Public License along with
// this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
//
////////////////////////////////////////////////////////////////////////
//
//   Author:  Hung-Wei Tseng, Yu-Chia Liu
//
//   Description:
//     Core of Kernel Module for Processor Container
//
////////////////////////////////////////////////////////////////////////

#include "memory_container.h"

#include <asm/uaccess.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/poll.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/kthread.h>

struct task_info {
    long long int cid;
    pid_t tid;
    struct task_info *next;
    struct task_info *prev;
    struct task_struct *taskinlist;         // pointing to its corresponding structure in kernel run queue
};

struct object {
    long long int oid;
    struct object *next;
    unsigned long long objspace;
};

struct container_info {
    struct task_info *head;
    struct task_info *foot;
    struct object *objref;
    //struct *object;
    // container will have a pointer to its array of objects
};

struct container_info *containers[1000];

struct mutex lockproc; 

DEFINE_MUTEX(lockproc);

/// calculate container id ///

long long int retcid(pid_t pid){

    printk(KERN_INFO "Finding container Id for pid %d",pid);
    
    long long int counter = 999999999;
    long long int i = 0;

    for(i = 0 ; i < 1000 ; i++){

        if(containers[i] != NULL) {

            if(containers[i]->head != NULL) {

                struct task_info *temptask = containers[i]->head;

                if(temptask->tid == pid){
                    printk(KERN_INFO "container id found %lld, in if ",i);

                    return i;
                }

                else {

                    temptask = temptask->next;

                    while(temptask->next != containers[i]->head->next){
                        if(temptask->tid == pid){
                            printk(KERN_INFO "container id found %lld, in while",i);
                            return i;
                        }

                        else {
                            temptask = temptask->next;
                        }
                    }
                }
            }
        }
    }

    printk(KERN_INFO "container id not found %lld ",counter);

    return counter;
}


/// map kernel memory to user memory ///

int memory_container_mmap(struct file *filp, struct vm_area_struct *vma)
{

    printk(KERN_INFO "mmap called");

    /*

    ////////    To generate the container of the currently running task  ///////
    
    */

    long long int oid = vma->vm_pgoff;
   	long long int osize = vma->vm_end - vma->vm_start;

    long long int counter = retcid(current->pid);

    if (counter == 999999999) {
        printk(KERN_INFO "No Container Found");
    }

    else {
        printk(KERN_INFO "mmap for task within the container of container id %lld ", counter);

        struct object *tempref = containers[counter]->objref;

        bool flag = false;

        if(tempref == NULL) {

            struct object *obj = kcalloc(1, sizeof(struct object), GFP_KERNEL);

            obj->oid = oid;
           // obj->lock = true;
            obj->next = NULL;
            obj->objspace = kcalloc(1, osize, GFP_KERNEL);

            printk(KERN_INFO "creating 1st object for container with container id %lld and object id %lld ",counter,oid);

            containers[counter]->objref = obj;
            tempref = containers[counter]->objref;
        }

        else if(tempref->oid != oid) { // checking if in case there exist only one object

            while(tempref->next != NULL){
                if(tempref->next->oid == oid){
                    tempref = tempref->next;    
                    flag = true;
                    //tempref->lock = true;
                    printk(KERN_INFO "object for container with container id %lld and object id %lld already exist",counter,oid);
                    break;
                }

                tempref = tempref->next;
            }

            if(!flag){
                printk(KERN_INFO "creating object for container with container id %lld and object id %lld ",counter,oid);
                
                struct object *obj = kcalloc(1, sizeof(struct object), GFP_KERNEL);

                obj->oid = oid;
                obj->next = NULL;
                obj->objspace = kcalloc(1, osize, GFP_KERNEL);

                tempref->next = obj;
                tempref = tempref->next;
            }
        }

        else {

            // 1st object is being referred
            printk(KERN_INFO "Object already here and is the 1st object");
        }

        //static char* kmallocarea = PAGE_ALIGN(&tempref);

        //char* kmallocarea = (int *)(((unsigned long)(tempref->objspace) + PAGE_SIZE -1) & PAGE_MASK);

        unsigned long long *kmallocarea;

        kmallocarea = PAGE_ALIGN(tempref->objspace);

        unsigned long pfn = virt_to_phys((void *)kmallocarea)>>PAGE_SHIFT;

        printk(KERN_INFO "physical memory location will be %x", pfn);

        unsigned long len = vma->vm_end - vma->vm_start;
        int ret ;

        ret = remap_pfn_range(vma, vma->vm_start, pfn, len, vma->vm_page_prot);
        if (ret < 0) {
            printk(KERN_INFO "could not map the address area\n");
            return -EIO;
        }
    }

    return 0;
}


/// create an object and lock it or just lock already created object ///

int memory_container_lock(struct memory_container_cmd __user *user_cmd)
{
    printk(KERN_INFO "locking the data structure");

    mutex_lock(&lockproc);

    return 0;
}

/// Unlock the object whos mapping has been done ///

int memory_container_unlock(struct memory_container_cmd __user *user_cmd)
{

    printk(KERN_INFO "unlocking the data structure");
    
    mutex_unlock(&lockproc);

    /*As allowing multiple access that is two task within a container can access different objects simultaneously hence need to make sure that after freeing an objec unlock would not be required because that object is free*/

    return 0;
}


int memory_container_delete(struct memory_container_cmd __user *user_cmd)
{
    mutex_lock(&lockproc);

    struct memory_container_cmd *temp;

    temp = kmalloc(sizeof(struct memory_container_cmd), GFP_KERNEL);

    copy_from_user(temp,user_cmd, sizeof(struct memory_container_cmd));

    printk(KERN_INFO "Function called to find container id");

    long long int counter = retcid(current->pid);      // in case switch gets called with all threads dead then counter won't be udpdated


    if (counter == 999999999) {
        printk(KERN_INFO "No Container Matched");
    }

    else {

        printk(KERN_INFO "to delete task with cid %lld", counter);

        struct task_info *tasktodelete = containers[counter]->head;

        if(tasktodelete->tid == current->pid && tasktodelete->next == tasktodelete){
            printk(KERN_INFO "deleting first and the last task of the container with id %lld",counter);
        
            kfree(tasktodelete);
            containers[counter]->head = NULL;
            containers[counter]->foot = NULL;
        }

        else if(tasktodelete->tid == current->pid) {
            printk(KERN_INFO "deleting first but not the last task of the container with id %lld",counter);
            
            containers[counter]->head = containers[counter]->head->next;
            containers[counter]->head->prev = containers[counter]->foot;
            containers[counter]->foot->next = containers[counter]->head;
            kfree(tasktodelete);
        }

        else if(current->pid == containers[counter]->foot->tid) {
            printk(KERN_INFO "deleting the last task of the container");
        
            containers[counter]->head->prev = containers[counter]->foot->prev;
            containers[counter]->foot->prev->next = containers[counter]->head;
            kfree(containers[counter]->foot);
            containers[counter]->foot = containers[counter]->head->prev;
        }   

        else {
            tasktodelete = tasktodelete->next;

            while(tasktodelete->next != containers[counter]->head){
                if(tasktodelete->tid == current->pid){
                    tasktodelete->prev->next = tasktodelete->next;
                    tasktodelete->next->prev = tasktodelete->prev;
                    kfree(tasktodelete);
                    break;
                }

                else {
                    tasktodelete = tasktodelete->next;
                }
            }
        }
    }    

    mutex_unlock(&lockproc);

    return 0;
}


int memory_container_create(struct memory_container_cmd __user *user_cmd)
{
    mutex_lock(&lockproc);

    struct memory_container_cmd *temp;

    temp = kmalloc(sizeof(struct memory_container_cmd), GFP_KERNEL);

    copy_from_user(temp,user_cmd, sizeof(struct memory_container_cmd));

    printk(KERN_INFO "to add task with cid %llu", temp->cid);

    struct task_info *task = kmalloc(sizeof(struct task_info), GFP_KERNEL);

    struct task_struct *currenttask = current;  // can directly use current->pid though.

    long long int x = temp->cid;    // check if _u64 = long long int

    if(containers[x] == NULL) {

            printk(KERN_INFO "new container");
            printk(KERN_INFO "creating task with id %d", currenttask->pid);
            task->cid = x;
            task->tid = currenttask->pid;
            task->taskinlist = currenttask;

            struct container_info *container = kmalloc(sizeof(struct container_info), GFP_KERNEL);

            containers[x] = container;

            containers[x]->head = task;
            containers[x]->foot = containers[x]->head;
            containers[x]->foot->next = containers[x]->head;
            containers[x]->head->prev = containers[x]->foot;
            containers[x]->objref = NULL;
        }

    else if (containers[x]->head == NULL) {
        // this is when a container has deleted all task which were created, then next task added as 1st task
        printk(KERN_INFO "container already here");
        printk(KERN_INFO "new first task of container, task id %d", currenttask->pid);
        task->cid = x;
        task->tid = currenttask->pid;
        task->taskinlist = currenttask;

        containers[x]->head = task;
        containers[x]->foot = containers[x]->head;
        containers[x]->foot->next = containers[x]->head;
        containers[x]->head->prev = containers[x]->head;
    }    

    else {

        printk(KERN_INFO "container already here");
        printk(KERN_INFO "creating task with id %d", currenttask->pid);
        task->cid = x;
        task->tid = currenttask->pid;
        task->taskinlist = currenttask;

        containers[x]->foot->next = task;
        containers[x]->foot->next->prev = containers[x]->foot;
        containers[x]->foot = task;
        containers[x]->foot->next = containers[x]->head;
        containers[x]->head->prev = containers[x]->foot;
    }

    mutex_unlock(&lockproc);

    return 0;
}


int memory_container_free(struct memory_container_cmd __user *user_cmd)
{

    struct memory_container_cmd *temp;    

    temp = kmalloc(sizeof(struct memory_container_cmd), GFP_KERNEL);

    copy_from_user(temp,user_cmd, sizeof(struct memory_container_cmd));

    //long long int x = temp->cid;

    printk(KERN_INFO "Function called to find container id");

    long long int counter = retcid(current->pid);      // in case switch gets called with all threads dead then counter won't be udpdated

    if (counter == 999999999) {
        printk(KERN_INFO "No Container Matched");
    }

    else {
        printk(KERN_INFO "freeing memory of container id %lld and object id %lld ",counter,temp->oid);

        struct object *tempref = containers[counter]->objref;

        // in case 1st object is deleted

        if(tempref == NULL){
            printk(KERN_INFO "No object left to free");
        }

        if(tempref->oid == temp->oid){

            if(tempref->next == NULL){
                printk(KERN_INFO "This is the 1st and the last object of the container");
                containers[counter]->objref = NULL;
                //kfree(tempref->objspace);
                kfree(tempref);
            }

            else {
                printk(KERN_INFO "This is the 1st object of the container");
                containers[counter]->objref = tempref->next;
               // kfree(tempref->objspace);
                kfree(tempref);
            }
        }

        else {
         
            printk(KERN_INFO "This is a middle object of the container");

            struct object *previous;

            while(tempref->oid != temp->oid && tempref->next != NULL) {
                previous = tempref;
                tempref = tempref->next;
            }

            printk(KERN_INFO "checking for error after freeing middle");

            if(tempref->oid == temp->oid) {
                previous->next = tempref->next;
                // kfree(tempref->objspace);
                kfree(tempref);

                printk(KERN_INFO "Freed the middle object");
            }

            else {
                printk(KERN_INFO "No such object found");
            }
        }
    }

    return 0;
}


/**
 * control function that receive the command in user space and pass arguments to
 * corresponding functions.
 */

int memory_container_ioctl(struct file *filp, unsigned int cmd,
                              unsigned long arg)
{
    switch (cmd)
    {
    case MCONTAINER_IOCTL_CREATE:
        return memory_container_create((void __user *)arg);
    case MCONTAINER_IOCTL_DELETE:
        return memory_container_delete((void __user *)arg);
    case MCONTAINER_IOCTL_LOCK:
        return memory_container_lock((void __user *)arg);
    case MCONTAINER_IOCTL_UNLOCK:
        return memory_container_unlock((void __user *)arg);
    case MCONTAINER_IOCTL_FREE:
        return memory_container_free((void __user *)arg);
    default:
        return -ENOTTY;
    }
}
