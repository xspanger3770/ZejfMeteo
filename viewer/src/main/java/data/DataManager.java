package data;

import data.computation.DirectComputation;
import data.computation.VariableComputation;
import exception.FatalApplicationException;
import exception.FatalIOException;
import exception.RuntimeApplicationException;
import main.ZejfMeteo;
import time.TimeUtils;

import java.io.*;
import java.util.*;
import java.util.concurrent.locks.Lock;
import java.util.concurrent.locks.ReadWriteLock;
import java.util.concurrent.locks.ReentrantReadWriteLock;

public class DataManager {

    public static final int PERMANENTLY_LOADED_HOURS = 24;
    public static final double VALUE_EMPTY = -999.0;
    public static final double VALUE_NOT_MEASURED = -998.0;
    private static final long DATA_LOAD_TIME = 15 * 1000 * 60L;
    private final List<DataHour> dataHours;

    private List<VariableComputation> variableCalculations;

    private DataHour lastDatahour = null;

    public static final File DATA_FOLDER = new File(ZejfMeteo.MAIN_FOLDER, "/data/");

    public static final File VARIABLE_CALCULATIONS_FILE = new File(DATA_FOLDER, "variable_calculations.dat");

    private final ReadWriteLock dataLock = new ReentrantReadWriteLock();

    private final Lock dataReadLock = dataLock.readLock();

    private final Lock dataWriteLock = dataLock.writeLock();

    public DataManager(){
        dataHours = new LinkedList<>();
        variableCalculations = new LinkedList<>();

        load();

        if(variableCalculations.isEmpty()){
            variableCalculations.add(new VariableComputation(11, 720, "T2M", "C", new DirectComputation()));
        }

        Timer timer = new Timer();
        TimerTask taskAutosave = new TimerTask() {
            @Override
            public void run() {
                try {
                    removeOld();
                    saveAll();
                }catch(FatalApplicationException e){
                    ZejfMeteo.handleException(e);
                }
            }
        };
        TimerTask taskCompute = new TimerTask() {
            @Override
            public void run() {
                dataWriteLock.lock();
                try {
                    for(DataHour dh:dataHours) {
                        if(dh.isComputationNeeded()) {
                            dh.runComputations(DataManager.this);
                        }
                    }
                } finally {
                    dataWriteLock.unlock();
                }
            }
        };

        // Schedule the task to run every two minutes, starting from now
        timer.schedule(taskAutosave, 30 * 1000, 2 * 60 * 1000);
        timer.schedule(taskCompute, 1000, 1000);
    }

    private void removeOld() throws FatalApplicationException {
        dataWriteLock.lock();
        try {
            Iterator<DataHour> iterator = dataHours.iterator();
            while(iterator.hasNext()){
                DataHour dataHour = iterator.next();
                if(System.currentTimeMillis() - dataHour.getLastUse() > DATA_LOAD_TIME){
                    saveHour(dataHour);
                    iterator.remove();
                }
            }
        }finally {
            dataWriteLock.unlock();
        }
    }

    public static final String[] MONTHS = new String[] { "January", "February", "March", "April", "May", "June", "July",
            "August", "September", "October", "November", "December" };

    public void load(){
        Calendar calendar = Calendar.getInstance();
        calendar.setTime(new Date());

        System.out.printf("Current hour number = %d\n", TimeUtils.getHourNumber(calendar));

        for(int i = 0; i < PERMANENTLY_LOADED_HOURS; i++){
            try {
                dataHourGet(calendar, true, true);
            } catch (FatalApplicationException | RuntimeApplicationException e) {
                ZejfMeteo.handleException(e);
            }

            calendar.add(Calendar.HOUR, -1);
        }

        try {
            loadVariableCalculations();
        } catch (FatalApplicationException e) {
            ZejfMeteo.handleException(e);
        }
    }

    @SuppressWarnings("unchecked")
    private void loadVariableCalculations() throws FatalApplicationException{
        try {
            if(!VARIABLE_CALCULATIONS_FILE.exists()){
                variableCalculations = new LinkedList<>();
                return;
            }

            ObjectInputStream in = new ObjectInputStream(new FileInputStream(VARIABLE_CALCULATIONS_FILE));
            variableCalculations = (List<VariableComputation>) in.readObject();
        } catch (IOException | ClassNotFoundException e) {
            throw new FatalApplicationException(e);
        }
    }


    private void saveVariableCalculations() throws FatalApplicationException {
        ObjectOutputStream out;
        try {
            out = new ObjectOutputStream(new FileOutputStream(VARIABLE_CALCULATIONS_FILE));
            out.writeObject(variableCalculations);
        } catch (IOException e) {
            throw new FatalApplicationException(e);
        }
    }

    public DataHour dataHourFind(long hourNumber){
        for(DataHour dataHour:dataHours){
            if(dataHour.getHourNumber() == hourNumber){
                return dataHour;
            }
        }
        return null;
    }

    public DataHour dataHourGet(long hourNumber, boolean load, boolean createNew) throws FatalApplicationException {
        if(lastDatahour != null && lastDatahour.getHourNumber() == hourNumber){
            return lastDatahour;
        }

        DataHour result = dataHourFind(hourNumber);

        boolean add = false;

        if(result == null && load){
            result = loadHour(TimeUtils.toCalendar(hourNumber));
            add = true;
        }

        if(result == null && createNew){
            result = new DataHour(hourNumber, this);
        }

        if(result != null && add){
            dataHours.add(result);
        }

        lastDatahour = result;

        return result;
    }

    public DataHour dataHourGet(Calendar calendar, boolean load, boolean createNew) throws FatalApplicationException{
        return dataHourGet(TimeUtils.getHourNumber(calendar), load, createNew);
    }

    public static File getDataHourFile(Calendar calendar){
        String str = String.format("/%d/%s/%02d/%02dH_%d.dat", calendar.get(Calendar.YEAR), MONTHS[calendar.get(Calendar.MONTH)],
                calendar.get(Calendar.DATE), calendar.get(Calendar.HOUR_OF_DAY), TimeUtils.getHourNumber(calendar));
        return new File(DATA_FOLDER, str);
    }

    private DataHour loadHour(Calendar calendar) throws FatalApplicationException {
        File file = getDataHourFile(calendar);
        System.out.println("Load: "+file.getAbsoluteFile());
        if(!file.exists()){
            return null;
        }

        try{
            ObjectInputStream in = new ObjectInputStream(new FileInputStream(file));
            DataHour result =(DataHour) in.readObject();
            result.runComputations(this);
            System.out.println("loaded "+result.getHourNumber()+", "+result.getVariablesRaw().size());
            return result;
        } catch(ClassNotFoundException | InvalidClassException e) {
            //throw new FatalApplicationException("Unable to load", e);
            File brokenFile = new File(file.getAbsolutePath() + "_broken");
            if(!file.renameTo(brokenFile)){
                throw new FatalApplicationException(new IOException("Unable to rename broken file "+brokenFile.getAbsolutePath()));
            }
            System.err.println((String.format("Could not load %s, renamed to %s", file.getName(), brokenFile.getName())));
            return null;
        } catch (IOException e) {
            throw new FatalIOException(e);
        }
    }

    private void saveHour(DataHour dataHour) throws FatalApplicationException {
        if(dataHour == null || !dataHour.isModified()){
            return;
        }
        Calendar calendar = TimeUtils.toCalendar(dataHour.getHourNumber());
        File file = getDataHourFile(calendar);
        if (!file.getParentFile().exists()) {
            if(!file.getParentFile().mkdirs()){
                throw new FatalApplicationException(new IOException("Unable to create directories for data"));
            }
        }
        System.out.println("Save: "+file.getAbsoluteFile());

        try {
            ObjectOutputStream out = new ObjectOutputStream(new FileOutputStream(file));
            out.writeObject(dataHour);
        } catch (IOException e) {
            throw new FatalApplicationException("Unable to save", e);
        }
    }

    public void saveAll() throws FatalApplicationException{
        dataReadLock.lock();
        try {
            for (DataHour dataHour : dataHours) {
                saveHour(dataHour);
            }
        }finally {
            dataReadLock.unlock();
        }

        saveVariableCalculations();
    }

    public void log(int variableId, int samplesPerHour, long hourNumber, int sampleNumber, double value) throws FatalApplicationException{
        dataWriteLock.lock();
        try{
            DataHour dataHour = dataHourGet(TimeUtils.toCalendar(hourNumber), true, true);
            dataHour.log(variableId, samplesPerHour, sampleNumber, value);
        }finally {
            dataWriteLock.unlock();
        }
    }

    public List<VariableComputation> getVariableCalculations() {
        return variableCalculations;
    }
}
