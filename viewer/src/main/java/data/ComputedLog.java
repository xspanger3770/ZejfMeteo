package data;

public class ComputedLog {

    private double value = 0.0;
    private ComputationStatus status = ComputationStatus.EMPTY;

    public double getValue() {
        return value;
    }

    public ComputationStatus getStatus() {
        return status;
    }

    public void setStatus(ComputationStatus status) {
        this.status = status;
    }

    public void setValue(double value) {
        this.value = value;
    }
}
