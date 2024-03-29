package data;

import data.computation.ComputationStatus;
import data.computation.ComputedLog;
import data.computation.ComputedVariable;
import data.computation.VariableComputation;
import exception.FatalApplicationException;
import exception.FatalIOException;
import exception.RuntimeApplicationException;
import main.ZejfMeteo;
import time.TimeUtils;

import java.io.*;
import java.util.*;
import java.util.concurrent.ConcurrentLinkedQueue;

public class DataManager {

    public static final int PERMANENTLY_LOADED_HOURS = 24;
    public static final double VALUE_EMPTY = -999.0;
    public static final double VALUE_NOT_MEASURED = -998.0;
    private static final long DATA_LOAD_TIME = 15 * 1000 * 60L;
    private final Queue<DataHour> dataHours;

    private List<VariableComputation> variableCalculations;

    private DataHour lastDatahour = null;

    public static final File DATA_FOLDER = new File(ZejfMeteo.MAIN_FOLDER, "/data/");

    public static final File VARIABLE_CALCULATIONS_FILE = new File(DATA_FOLDER, "variable_calculations.dat");

    public DataManager(){
        dataHours = new ConcurrentLinkedQueue<>();
        variableCalculations = new LinkedList<>();

        load();

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
                for(DataHour dh:dataHours) {
                    if(dh.isComputationNeeded()) {
                        dh.runComputations(DataManager.this);
                    }
                }
            }
        };

        TimerTask taskDataChecks = new TimerTask() {
            @Override
            public void run() {
                for(DataHour dh:dataHours) {
                    dh.runDataChecks();
                }
            }
        };

        // Schedule the task to run every two minutes, starting from now
        timer.schedule(taskAutosave, 30 * 1000, 2 * 60 * 1000);
        timer.schedule(taskDataChecks, 1000, 1000 );
        timer.schedule(taskCompute, 1000, 1000);
    }

    private void removeOld() throws FatalApplicationException {
        Iterator<DataHour> iterator = dataHours.iterator();
        while(iterator.hasNext()){
            DataHour dataHour = iterator.next();
            if(System.currentTimeMillis() - dataHour.getLastUse() > DATA_LOAD_TIME){
                saveHour(dataHour);
                iterator.remove();;
            }
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
                dataHourGet(calendar, true);
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

    private DataHour dataHourFind(long hourNumber){
        for (DataHour dataHour : dataHours) {
            if (dataHour.getHourNumber() == hourNumber) {
                return dataHour;
            }
        }

        return null;
    }

    public DataHour dataHourGet(long hourNumber, boolean load) throws FatalApplicationException {
        if (lastDatahour != null && lastDatahour.getHourNumber() == hourNumber) {
            return lastDatahour;
        }

        DataHour result = dataHourFind(hourNumber);

        boolean add = false;

        if (result == null && load) {
            result = loadHour(TimeUtils.toCalendar(hourNumber));
            add = true;
        }

        if (result == null) {
            result = new DataHour(hourNumber, this);
            add = true;
        }

        if (add) {
            dataHours.add(result);
        }

        lastDatahour = result;
        return result;
    }

    public DataHour dataHourGet(Calendar calendar, boolean load) throws FatalApplicationException{
        return dataHourGet(TimeUtils.getHourNumber(calendar), load);
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
            dataHour.saved();
        } catch (IOException e) {
            throw new FatalApplicationException("Unable to save", e);
        }
    }

    public void saveAll() throws FatalApplicationException{
        for (DataHour dataHour : dataHours) {
            saveHour(dataHour);
        }

        saveVariableCalculations();
    }

    public void log(int variableId, int samplesPerHour, long hourNumber, int sampleNumber, double value) throws FatalApplicationException{
        DataHour dataHour = dataHourGet(TimeUtils.toCalendar(hourNumber), true);
        dataHour.log(variableId, samplesPerHour, sampleNumber, value);
    }

    public List<VariableComputation> getVariableCalculations() {
        return variableCalculations;
    }

    public ComputedLog getLastValue(VariableComputation computation) {
        try {
            Calendar calendar = Calendar.getInstance();
            calendar.setTime(new Date());
            for(int i = 0; i < 24; i++){
                DataHour dh = dataHourGet(calendar, true);
                if(dh == null){
                    calendar.add(Calendar.HOUR, -1);
                    continue;
                }
                ComputedVariable computedVariable = dh.getComputedVariable(computation.getUuid(), computation.getSamplesPerHour(), false);
                if(computedVariable == null){
                    calendar.add(Calendar.HOUR, -1);
                    continue;
                }

                ComputedLog computedLog = computedVariable.getLastLog();
                if(computedLog.getStatus() == ComputationStatus.FINISHED){
                    return computedLog;
                }

                calendar.add(Calendar.HOUR, -1);
            }
        } catch (FatalApplicationException e) {
            ZejfMeteo.handleException(e);
        }

        return null;
    }

    public ComputedLog getValue(VariableComputation computation, Calendar calendar){
        try {
            DataHour dh = dataHourGet(calendar, true);
            if(dh == null){
                return null;
            }
            ComputedVariable computedVariable = dh.getComputedVariable(computation.getUuid(), computation.getSamplesPerHour(), false);
            if(computedVariable == null){
                return null;
            }

            return computedVariable.getComputedLogs()[TimeUtils.getSampleNumber(calendar, computation.getSamplesPerHour())];
        } catch (FatalApplicationException e) {
            ZejfMeteo.handleException(e);
        }

        return null;
    }

    public void create(int variableId, int samplesPerHour, long hourNumber) throws FatalApplicationException{
        DataHour dataHour = dataHourGet(TimeUtils.toCalendar(hourNumber), true);
        if(dataHour != null){
            dataHour.getVariable(variableId, samplesPerHour, true);
        }
    }
}
