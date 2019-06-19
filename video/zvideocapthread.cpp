#include "zvideocapthread.h"
#include "zgblpara.h"
#include <QDebug>

#ifdef ZSY_USE_CUDA
#include <cuda_runtime.h>
#include "yuv2rgb.cuh"
#endif

ZVideoCapThread::ZVideoCapThread(QString videoNodeName)
{
    this->m_videoNodeName=videoNodeName;
    this->m_bCleanUp=false;
}
int ZVideoCapThread::ZIoctl(int fd,unsigned long int request,void *arg)
{
    int ret;
    do{
        ret=ioctl(fd,request,arg);
    }while (-1==ret && EINTR==errno);
    return ret;
}
void ZVideoCapThread::ZDoClean(void)
{
    qDebug()<<"<VIDEO_CAP>:thread done.";
    //set global request exit flag to notify other threads.
    gGblPara.m_bAppRst2ExitFlag=true;
    this->m_bCleanUp=true;
    emit this->ZSigFinished();
}
void ZVideoCapThread::run()
{
    int fd=open(this->m_videoNodeName.toStdString().c_str(),O_RDWR|O_NONBLOCK,0);
    if(fd<0)
    {
        qDebug()<<"<error>:failed to open video node"<<this->m_videoNodeName;
        return;
    }

    struct v4l2_capability cap;
    struct v4l2_crop crop;
    if(this->ZIoctl(fd,VIDIOC_QUERYCAP,&cap)<0)
    {
        qDebug()<<"<error>:failed to query capability"<<this->m_videoNodeName;
        this->ZDoClean();
        return;
    }

    if(!(cap.capabilities&V4L2_CAP_VIDEO_CAPTURE))
    {
        qDebug()<<"<error>:node doesnot support video capture"<<this->m_videoNodeName;
        this->ZDoClean();
        return;
    }

    if(!(cap.capabilities&V4L2_CAP_STREAMING))
    {
        qDebug()<<"<error>:node doesnot support streaming capture"<<this->m_videoNodeName;
        this->ZDoClean();
        return;
    }


    /* Select video input, video standard and tune here. */
    struct v4l2_cropcap cropcap;
    CLEAR(cropcap);
    cropcap.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if(0==this->ZIoctl(fd,VIDIOC_CROPCAP,&cropcap))
    {
        crop.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
        crop.c=cropcap.defrect; /* reset to default */
        if(this->ZIoctl(fd,VIDIOC_S_CROP,&crop)<0)
        {
            switch (errno)
            {
            case EINVAL:
                /* Cropping not supported. */
                break;
            default:
                /* Errors ignored. */
                break;
            }
        }
    }else{
        /* Errors ignored. */
        qDebug()<<"<error>:happened in VIDIOC_CROPCAP"<<this->m_videoNodeName;
    }

    struct v4l2_format fmt;
    CLEAR(fmt);
    fmt.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width=1280;
    fmt.fmt.pix.height=720;
    fmt.fmt.pix.pixelformat=V4L2_PIX_FMT_YUYV;
    fmt.fmt.pix.field=V4L2_FIELD_INTERLACED;

    if(this->ZIoctl(fd,VIDIOC_S_FMT,&fmt)<0)
    {
        qDebug()<<"<error>:happened in VIDIOC_S_FMT"<<this->m_videoNodeName;
    }
    /* Note VIDIOC_S_FMT may change width and height. */

#ifdef ZSY_USE_CUDA
    /* Check unified memory support. */
    if (cuda_zero_copy)
    {
        cudaDeviceProp devProp;
        cudaGetDeviceProperties (&devProp, 0);
        if (!devProp.managedMemory) {
            printf ("CUDA device does not support managed memory.\n");
            cuda_zero_copy = false;
        }
    }


    /* Allocate output buffer. */
    size_t size = width * height * 3;
    if (cuda_zero_copy) {
        cudaMallocManaged (&cuda_out_buffer, size, cudaMemAttachGlobal);
    } else {
        cuda_out_buffer = (unsigned char *) malloc (size);
    }

    cudaDeviceSynchronize ();
#endif

    /* Buggy driver paranoia. */
    unsigned int min;
    min = fmt.fmt.pix.width * 2;
    if (fmt.fmt.pix.bytesperline < min)
        fmt.fmt.pix.bytesperline = min;
    min = fmt.fmt.pix.bytesperline * fmt.fmt.pix.height;
    if (fmt.fmt.pix.sizeimage < min)
        fmt.fmt.pix.sizeimage = min;

    //IO_METHOD_MMAP

    //IO_METHOD_USERPTR
    int buffer_size=static_cast<int>(fmt.fmt.pix.sizeimage);
    int page_size=getpagesize();
    buffer_size=(buffer_size+page_size-1)&~(page_size-1);

    struct v4l2_requestbuffers req;
    CLEAR(req);
    req.count=4;
    req.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory=V4L2_MEMORY_USERPTR;
    if(this->ZIoctl(fd,VIDIOC_REQBUFS,&req)<0)
    {
        qDebug()<<"<error>:"<<this->m_videoNodeName<<"does not support user pointer I/O.";
        this->ZDoClean();
        return;
    }

    struct buffer *buffers;
    buffers=static_cast<struct buffer *>(calloc(4,sizeof(*buffers)));
    if(!buffers)
    {
        qDebug()<<"<error>:"<<this->m_videoNodeName<<"out of memory!";
        this->ZDoClean();
        return;
    }

    for(unsigned int i=0;i<4;++i)
    {
        buffers[i].length=static_cast<unsigned long>(buffer_size);
#ifdef ZSY_USE_CUDA
        if(cuda_zero_copy)
        {
            cudaMallocManaged (&buffers[i].start,buffer_size,cudaMemAttachGlobal);
        } else {
            buffers[i].start=memalign(/*boundary*/page_size,buffer_size);
        }
#else
        buffers[i].start=memalign(/*boundary*/static_cast<size_t>(page_size),static_cast<size_t>(buffer_size));
#endif
        if(!buffers[i].start)
        {
            qDebug()<<"<error>:"<<this->m_videoNodeName<<"out of memory!";
            this->ZDoClean();
            return;
        }
    }

    //start capture.
    enum v4l2_buf_type type;
    //IO_METHOD_USERPTR
    for(unsigned int i=0;i<4;++i)
    {
        struct v4l2_buffer buf;
        CLEAR(buf);
        buf.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory=V4L2_MEMORY_USERPTR;
        buf.index=i;
        buf.m.userptr=reinterpret_cast<unsigned long>(buffers[i].start);
        buf.length=static_cast<__u32>(buffers[i].length);
        if(this->ZIoctl(fd,VIDIOC_QBUF,&buf)<0)
        {
            qDebug()<<"<error>:"<<this->m_videoNodeName<<"failed to VIDIOC_QBUF!";
            this->ZDoClean();
            return;
        }
    }

    type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if(this->ZIoctl(fd,VIDIOC_STREAMON,&type)<0)
    {
        qDebug()<<"<error>:"<<this->m_videoNodeName<<"failed to VIDIOC_STREAMON!";
        this->ZDoClean();
        return;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    while(!gGblPara.m_bAppRst2ExitFlag)
    {
        fd_set fds;
        struct timeval tv;
        int ret;

        FD_ZERO(&fds);
        FD_SET(fd,&fds);

        /* Timeout. */
        tv.tv_sec=2;
        tv.tv_usec=0;
        ret=select(fd+1,&fds,NULL,NULL,&tv);
        if(ret<0)
        {
            qDebug()<<"<error>:"<<this->m_videoNodeName<<"failed to select()!";
            break;
        }else if(ret==0)
        {
            //select timeout.
            continue;
        }else{
            //IO_METHOD_USERPTR
            struct v4l2_buffer buf;
            CLEAR(buf);
            buf.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory=V4L2_MEMORY_USERPTR;
            if(this->ZIoctl(fd,VIDIOC_DQBUF,&buf)<0)
            {
                qDebug()<<"<error>:"<<this->m_videoNodeName<<"failed to VIDIOC_DQBUF!";
            }
            //            unsigned int i;
            //            for(i=0;i<n_buffers;++i)
            //                if (buf.m.userptr == (unsigned long) buffers[i].start && buf.length == buffers[i].length)
            //                    break;

            //            assert (i<n_buffers);

            //process_image((void*)buf.m.userptr);


            if(this->ZIoctl(fd,VIDIOC_QBUF,&buf)<0)
            {
                qDebug()<<"<error>:"<<this->m_videoNodeName<<"failed to VIDIOC_QBUF!";
            }
        }
    }
    ////////////////////////////////////////////////////////////////////////////////////////////////////

    //stop capture.
    type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if(this->ZIoctl(fd,VIDIOC_STREAMOFF,&type)<0)
    {
        qDebug()<<"<error>:"<<this->m_videoNodeName<<"failed to VIDIOC_STREAMOFF!";
    }
    //IO_METHOD_USERPTR.
#ifdef ZSY_USE_CUDA
    for (unsigned int i = 0; i < 4; ++i) {
        if (cuda_zero_copy) {
            cudaFree (buffers[i].start);
        } else {
            free (buffers[i].start);
        }
    }
#else
    for (unsigned int i=0;i<4;++i)
    {
        free(buffers[i].start);
    }
#endif
    free(buffers);
    close(fd);
    this->ZDoClean();
    return;
}
