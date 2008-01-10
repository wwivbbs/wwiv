package com.wwiv.bbs.legacy.datafile;

import java.io.Closeable;
import java.io.DataInput;
import java.io.EOFException;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.RandomAccessFile;

public class DataFileInput implements Closeable {
    private final RandomAccessFile raf;
    public DataFileInput(File file) throws FileNotFoundException {
        raf = new RandomAccessFile(file, "r");
    }

    public void readFully(byte[] b) throws IOException {
        raf.readFully(b, 0, b.length);
    }

    public void readFully(byte[] b, int off, int len) throws IOException {
        raf.readFully(b, off, len);
    }

    public int skipBytes(int n) throws IOException {
        return raf.skipBytes(n);
    }

    public boolean readBoolean() throws IOException {
        return raf.readBoolean();
    }

    public byte readByte() throws IOException {
        return raf.readByte();
    }

    public int readUnsignedByte() throws IOException {
        return raf.readUnsignedByte();
    }

    public short readShort() throws IOException {
        int ch1 = raf.read();
        int ch2 = raf.read();
        if ((ch1 | ch2) < 0)
            throw new EOFException();
        return (short)((ch1 << 0) + (ch2 << 8));
    }

    public int readUnsignedShort() throws IOException {
        int ch1 = raf.read();
        int ch2 = raf.read();
        if ((ch1 | ch2) < 0)
            throw new EOFException();
        return (ch1 << 0) + (ch2 << 8);    
    }

    public int readInt()throws IOException {
        int ch1 = raf.read();
        int ch2 = raf.read();
        int ch3 = raf.read();
        int ch4 = raf.read();
        if ((ch1 | ch2 | ch3 | ch4) < 0)
            throw new EOFException();
        return ((ch4 << 24) + (ch3 << 16) + (ch2 << 8) + (ch1 << 0));
    }

    public float readFloat() throws IOException {
        return Float.intBitsToFloat(readInt());
    }

    /**
     * Reads a C style string from a DataInput into a Java String.
     * @param dataInput
     * @param stringLength
     * @return The Java string from the DataInput
     * @throws IOException on error
     * @throws EOFException on unexpected EOF
     */
    public String readCString(int stringLength) throws IOException, 
                                                              EOFException {
        final byte b[] = new byte[stringLength];
        raf.readFully(b, 0, stringLength);
        for (int i = 0; i < stringLength; i++) {
            if (b[i] == 0) {
                return new String(b, 0, i);
            }
        }
        return new String(b, 0, stringLength);
    }


    public void close() throws IOException{
        raf.close();
    }
}
