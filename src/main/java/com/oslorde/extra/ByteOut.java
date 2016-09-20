package com.oslorde.extra;


public class ByteOut {
    /**
     * The byte array containing the bytes written.
     */
    private byte[] buf;

    /**
     * The number of bytes written.
     */
    private int count;

    /**
     * Constructs a new ByteArrayOutputStream with a default size of 32 bytes.
     * If more than 32 bytes are written to this instance, the underlying byte
     * array will expand.
     */
    public ByteOut() {
        buf = new byte[128];
    }

    /**
     * Constructs a new {@code ByteArrayOutputStream} with a default size of
     * {@code size} bytes. If more than {@code size} bytes are written to this
     * instance, the underlying byte array will expand.
     *
     * @param size initial size for the underlying byte array, must be
     *             non-negative.
     * @throws IllegalArgumentException if {@code size} < 0.
     */
    public ByteOut(int size) {
        if (size >= 0) {
            buf = new byte[size];
        } else {
            throw new IllegalArgumentException("size < 0");
        }
    }

    /**
     * Returns the contents of this ByteArrayOutputStream as a byte array. Any
     * changes made to the receiver after returning will not be reflected in the
     * byte array returned to the caller.
     *
     * @return this stream's current contents as a byte array.
     */
    public synchronized byte[] toByteArray() {
        byte[] newArray = new byte[count];
        System.arraycopy(buf, 0, newArray, 0, count);
        return newArray;
    }

    /**
     * Returns the total number of bytes written to this stream so far.
     *
     * @return the number of bytes written to this stream.
     */
    public int size() {
        return count;
    }

    public void reset() {
        count = 0;
    }

    /**
     * Writes the specified byte {@code oneByte} to the OutputStream. Only the
     * low order byte of {@code oneByte} is written.
     *
     * @param c the byte to be written.
     */
    public synchronized void write(char c) {
        if (count == buf.length) {
            expand(1);
        }
        buf[count++] = (byte) (c & 0xff);
        if (c > 0xff) {
            buf[count++] = (byte) (c >> 8);
        }
    }

    /**
     * Writes the specified byte {@code oneByte} to the OutputStream. Only the
     * low order byte of {@code oneByte} is written.
     *
     * @param oneByte the byte to be written.
     */
    public synchronized void write(byte oneByte) {
        if (count == buf.length) {
            expand(1);
        }
        buf[count++] = oneByte;
    }

    /**
     * Writes {@code count} bytes from the byte array {@code buffer} starting at
     * offset {@code index} to this stream.
     *
     * @param buffer the buffer to be written.
     * @param offset the initial position in {@code buffer} to retrieve bytes.
     * @param len    the number of bytes of {@code buffer} to write.
     * @throws NullPointerException      if {@code buffer} is {@code null}.
     * @throws IndexOutOfBoundsException if {@code offset < 0} or {@code len < 0}, or if
     *                                   {@code offset + len} is greater than the length of
     *                                   {@code buffer}.
     */
    public void write(byte[] buffer, int offset, int len) {
        int bufLen = buffer.length;
        if ((offset | len) < 0 || offset > bufLen || bufLen - offset < len) {
            throw new ArrayIndexOutOfBoundsException("length=" + bufLen + "; regionStart=" + offset
                    + "; regionLength=" + len);
        }
        if (len == 0) {
            return;
        }
        expand(len);
        System.arraycopy(buffer, offset, buf, this.count, len);
        this.count += len;
    }

    /**
     * Writes byteArray {@code buffer} to this stream.
     *
     * @param buffer the buffer to be written.
     * @throws NullPointerException if {@code buffer} is {@code null}.
     */
    public void write(byte[] buffer) {
        int len = buffer.length;
        if (len == 0) {
            return;
        }
        expand(len);
        System.arraycopy(buffer, 0, buf, this.count, len);
        this.count += len;
    }

    private void expand(int i) {
        /* Can the buffer handle @i more bytes, if not expand it */
        if (count + i <= buf.length) {
            return;
        }

        byte[] newbuf = new byte[(count + i) << 1];
        System.arraycopy(buf, 0, newbuf, 0, count);
        buf = newbuf;
    }

}
