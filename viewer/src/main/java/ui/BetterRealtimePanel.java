package ui;

import data.computation.ComputedLog;
import data.computation.VariableComputation;
import main.ZejfMeteo;

import javax.swing.*;
import java.awt.*;
import java.util.Timer;
import java.util.TimerTask;

public class BetterRealtimePanel extends JPanel {

    public BetterRealtimePanel(){
        java.util.Timer timer = new Timer();

        TimerTask task = new TimerTask() {
            @Override
            public void run() {
                BetterRealtimePanel.super.repaint();
            }
        };

        // Schedule the task to run every two minutes, starting from now
        timer.schedule(task, 1000, 1000);
    }

    @Override
    public void paint(Graphics gr) {
        super.paint(gr);
        Graphics2D g = (Graphics2D) gr;
        g.setColor(Color.black);
        g.setFont(new Font("Calibri", Font.BOLD, 14));

        int y = 20;

        for(VariableComputation computation: ZejfMeteo.getDataManager().getVariableCalculations()){
            ComputedLog log = ZejfMeteo.getDataManager().getLastValue(computation);
            double val = log == null ? 0.0 : log.getValue();
            g.drawString(String.format("%s: %.3f%s", computation.getDisplayName(), val, computation.getUnits()), 2, y);
            y += 22;
        }
    }

}
