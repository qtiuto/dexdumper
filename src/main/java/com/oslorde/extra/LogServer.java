package com.oslorde.extra;

import android.util.SparseArray;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.ServerSocket;
import java.net.Socket;
import java.nio.charset.Charset;
import java.util.concurrent.SynchronousQueue;
import java.util.concurrent.ThreadPoolExecutor;
import java.util.concurrent.TimeUnit;


public final class LogServer {
    public static final int LOG_CONFIG=8;
    private static final int LOG_PORT= 12786;
    private static LogServer inst;
    private ThreadPoolExecutor pool;
    private boolean running;
    private ServerSocket server;
    private String storePath;

    private LogServer() {
    }
    public static LogServer getInstance(){
        if(inst==null) inst=new LogServer();
        return inst;
    }

    void start()  {
        if(running) return;
        running=true;
        pool=new ThreadPoolExecutor(1,20,5, TimeUnit.SECONDS,new SynchronousQueue<Runnable>());
        pool.submit(new Runnable() {
            @Override
            public void run() {
                try {
                    server=new ServerSocket(LOG_PORT);
                    while (true){
                        final Socket socket=server.accept();
                        if(socket==null) break;
                        socket.setKeepAlive(true);
                        pool.submit(new Runnable() {
                            @Override
                            public void run() {
                                SparseArray<OutputStream> streams=new SparseArray<OutputStream>();
                                try {
                                    InputStream input= new TimeOutInputStream(socket.getInputStream());
                                    byte[] header=new byte[16];
                                    int read;
                                    while (!socket.isClosed()&&!socket.isInputShutdown()){
                                        read=input.read(header);
                                        if(read>=0&&read<16){
                                            throw new IOException("Bad Data With Len="+read);
                                        }
                                        if(read<0){
                                            throw new IOException("Socket may be shutdown");
                                        }
                                        int priority= readIntFromByteArray(header,0);
                                        int tagLen=readIntFromByteArray(header,4);
                                        int msgLen=readIntFromByteArray(header,8);
                                        int thread_id=readIntFromByteArray(header,12);
                                        byte[] buffer=new byte[tagLen+msgLen];
                                        read=input.read(buffer);
                                        if (read!=buffer.length){
                                            throw new IOException("Bad Msg");
                                        }
                                        String tag=new String(buffer,0,tagLen, Charset.forName("UTF-8"));
                                        String msg=new String(buffer,tagLen,msgLen,Charset.forName("UTF-8"));

                                        if(priority==LOG_CONFIG){
                                            File f=new File(msg);
                                            if(tag.equals("storePath")&&storePath==null&&f.exists()&&f.canWrite()){
                                                storePath=msg;
                                                Utils.log("storePath="+storePath);
                                            }
                                        }else {
                                            //noinspection WrongConstant
                                            //Log.println(priority,tag,msg);
                                            if(storePath!=null){
                                                try {
                                                    OutputStream output;
                                                    if((output=streams.get(thread_id))==null){
                                                        output=new FileOutputStream(new File(storePath,"log_"+thread_id+".txt"));
                                                        streams.put(thread_id,output);
                                                    }
                                                    output.write(tag.getBytes());
                                                    output.write(": ".getBytes());
                                                    output.write(msg.getBytes());
                                                    output.write('\n');
                                                    output.flush();
                                                }catch (Exception ignored){}
                                            }

                                        }


                                    }
                                } catch (IOException e) {
                                    e.printStackTrace();
                                    System.out.println("Socket state: closed="+socket.isClosed()+", input="+!socket.isInputShutdown());
                                }finally {
                                    for (int i=0;i<streams.size();i++){
                                        try {
                                            streams.valueAt(i).close();
                                        } catch (IOException e) {
                                            e.printStackTrace();
                                        }
                                    }
                                    streams.clear();
                                }
                            }
                        });
                    }
                } catch (Exception e) {
                    e.printStackTrace();
                }
                running=false;
                if(!server.isClosed()){
                    shutdown();
                }
            }
        });
    }
    //Little endian
    public static int readIntFromByteArray(byte[] arr,int start){
        if(arr.length<start+4) throw new IndexOutOfBoundsException("This byte array can't read int any more");
        return (arr[start+3]&0xff)<<24|(arr[start+2]&0xff)<<16|(arr[start+1]&0xff)<<8|(arr[start]&0xff);
    }
    public boolean isRunning(){return running;}

    public void shutdown(){
        try {
            if(server!=null){
                server.close();
                server=null;
            }
            if(pool!=null){
                pool.shutdown();
                pool=null;
            }
        } catch (IOException e) {
            e.printStackTrace();
        }
    }
}
