package data;

import java.io.Serializable;
import java.util.Arrays;

public class DataVariable implements Serializable {

    private final int id;
    private final int samplesPerHour;

    private int lastLog = 0;

    private final double[] data;

    private transient long lastDataCheck = 0;

    public DataVariable(int id, int samplesPerHour){
        this.id = id;
        this.samplesPerHour = samplesPerHour;
        this.data = new double[samplesPerHour];
        Arrays.fill(data, DataManager.VALUE_EMPTY);
    }

    public int getId() {
        return id;
    }

    public int getSamplesPerHour() {
        return samplesPerHour;
    }

    public double[] getData() {
        return data;
    }

    public boolean log(int sampleNumber, double value) {
        if(sampleNumber < 0 || sampleNumber >= samplesPerHour){
            return false;
        }

        if (data[sampleNumber] != DataManager.VALUE_EMPTY &&
                data[sampleNumber] != DataManager.VALUE_NOT_MEASURED &&
                        (value == DataManager.VALUE_EMPTY || value == DataManager.VALUE_NOT_MEASURED)) {
            return false; // cannot rewrite wrong value
        }

        //System.out.println("Logged "+value+" ["+id+"@"+samplesPerHour+"]");

        data[sampleNumber] = value;
        if(sampleNumber > lastLog){
            lastLog = sampleNumber;
        }

        return true;
    }

    public int calculateDataCheck(){
        int result = 0;
        for(double val:data){
            if(val != DataManager.VALUE_EMPTY && val != DataManager.VALUE_NOT_MEASURED){
                result++;
            }
        }

        lastDataCheck = System.currentTimeMillis();

        return result;
    }

    public long getLastDataCheck() {
        return lastDataCheck;
    }
}
