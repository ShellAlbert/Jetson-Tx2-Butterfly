#ifndef ZVIDEOCAPTHREAD_H
#define ZVIDEOCAPTHREAD_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <getopt.h>             /* getopt_long() */

#include <fcntl.h>              /* low-level i/o */
#include <unistd.h>
#include <errno.h>
#include <malloc.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include <asm/types.h>          /* for videodev2.h */

#include <linux/videodev2.h>
#define CLEAR(x) memset (&(x), 0, sizeof (x))
#define ARRAY_SIZE(a)   (sizeof(a)/sizeof((a)[0]))

#include <QThread>

struct buffer {
    void *                  start;
    size_t                  length;
};
class ZVideoCapThread : public QThread
{
    Q_OBJECT
public:
    ZVideoCapThread(QString videoNodeName);
protected:
    void run();
signals:
    void ZSigFinished();
private:
    int ZIoctl(int fd,unsigned long int request,void *arg);
    void ZDoClean(void);
private:
    bool m_bCleanUp;
    QString m_videoNodeName;
};

#endif // ZVIDEOCAPTHREAD_H
