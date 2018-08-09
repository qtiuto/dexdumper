package com.oslorde.extra;

import java.io.BufferedInputStream;
import java.io.IOException;
import java.io.InputStream;



public class TimeOutInputStream extends BufferedInputStream{
    private long timeout=100;
    public TimeOutInputStream( InputStream in){
        super(in);
    }

    public TimeOutInputStream(InputStream in, long timeout) {
        super(in);
        if(timeout>0) this.timeout=timeout;
    }

    public TimeOutInputStream( InputStream in, int size, long timeout) {
        super(in, size);
        if(timeout>0) this.timeout=timeout;
    }

    @Override
    public synchronized int read( byte[] b, int off, int len) throws IOException {
        long deadline=System.currentTimeMillis()+timeout;
        int read=0;
        while (in.available()>=0&&System.currentTimeMillis()<deadline&&read<len){
             read+=super.read(b,off+read,len-read);
        }
        int avail;
        if(read<len&&(avail=read+count-pos)<buf.length){
            System.arraycopy(buf,pos,buf,read,avail);
            System.arraycopy(b,off,buf,0,read);
            pos=0;
            count=read+avail;
            read=0;
        }
        return read;
    }
}
