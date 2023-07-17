package ui;

import exception.FatalIOException;
import exception.RuntimeApplicationException;
import main.ZejfMeteo;
import main.Settings;
import socket.SocketManager;

import javax.swing.*;
import javax.swing.border.EmptyBorder;
import java.awt.*;
import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;

public class ZejfFrame extends JFrame {

    private final SocketManager socketManager;
    private final JLabel lblStatus;
    private JPanel realtimePanel;

    public ZejfFrame(){
        setTitle("ZejfMeteoViewer "+ ZejfMeteo.VERSION);
        getContentPane().setPreferredSize(new Dimension(600, 400));
        setResizable(true);
        setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);

        getContentPane().add(createTabbedPane());
        setJMenuBar(createMenuBar());

        lblStatus = new JLabel("Initializing...");
        lblStatus.setFont(new Font("Tahoma", Font.BOLD, 13));
        lblStatus.setBorder(new EmptyBorder(2, 4, 2, 2));
        getContentPane().add(lblStatus, BorderLayout.SOUTH);

        pack();
        setLocationRelativeTo(null);

        socketManager = new SocketManager();
    }

    public void setStatus(String status){
        if(lblStatus != null){
            lblStatus.setText(status);
        }
    }

    private JMenuBar createMenuBar() {
        JMenuBar jMenuBar = new JMenuBar();

        JMenu menuConnection = new JMenu("Connection");

        JMenuItem socketItem = new JMenuItem("Socket");
        socketItem.addActionListener(actionEvent -> {
            if(socketManager.isSocketRunning()){
                int result = JOptionPane.showConfirmDialog(ZejfFrame.this,
                        "Disconnect from " + Settings.ADDRESS + ":" + Settings.PORT + "?", "Server",
                        JOptionPane.YES_NO_OPTION);
                if (result != 0) {
                    return;
                }

                try {
                    socketManager.close();
                } catch(RuntimeApplicationException e){
                    ZejfMeteo.handleException(e);
                }
            }

            if(selectAddress()){
                try {
                    socketManager.connect();
                }catch(RuntimeApplicationException e){
                    ZejfMeteo.handleException(e);
                }
            }
        });

        menuConnection.add(socketItem);

        JMenuItem itemComputations = new JMenuItem("Manage Variables");
        itemComputations.addActionListener(new ActionListener() {
            @Override
            public void actionPerformed(ActionEvent actionEvent) {
                new ComputationsManager(ZejfFrame.this).setVisible(true);
            }
        });

        JMenu menuOptions = new JMenu("Options");
        menuOptions.add(itemComputations);

        menuOptions.add(new JMenuItem("Settings"));

        jMenuBar.add(menuConnection);
        jMenuBar.add(menuOptions);

        return jMenuBar;
    }

    private boolean selectAddress() {
        String[] labels = { "Address:", "Port:" };
        int numPairs = labels.length;
        JTextField[] fields = new JTextField[numPairs];
        String[] values = new String[numPairs];
        values[0] = Settings.ADDRESS;
        values[1] = String.valueOf(Settings.PORT);
        JPanel p = new JPanel(new SpringLayout());
        for (int i = 0; i < numPairs; i++) {
            JLabel l = new JLabel(labels[i], JLabel.TRAILING);
            p.add(l);
            JTextField textField = new JTextField(3);
            textField.setText(values[i]);
            l.setLabelFor(textField);
            p.add(textField);
            fields[i] = textField;
        }
        SpringUtilities.makeCompactGrid(p, numPairs, 2, // rows, cols
                6, 6, // initX, initY
                6, 6); // xPad, yPad
        if (JOptionPane.showConfirmDialog(this, p, "Select Server", JOptionPane.OK_CANCEL_OPTION,
                JOptionPane.PLAIN_MESSAGE) == 0) {
            try {
                String address = fields[0].getText();
                int port = Integer.parseInt(fields[1].getText());
                Settings.ADDRESS = address;
                Settings.PORT = port;
                Settings.saveProperties();
            } catch (NumberFormatException ex) {
                JOptionPane.showMessageDialog(this, "Error: " + ex.getMessage(), "Error", JOptionPane.ERROR_MESSAGE);
                selectAddress();
            } catch (FatalIOException e) {
                ZejfMeteo.handleException(e);
            }
            return true;
        }
        return false;
    }

    JTabbedPane createTabbedPane(){
        JTabbedPane tabbedPane = new JTabbedPane();

        tabbedPane.addTab("Realtime", realtimePanel = new BetterRealtimePanel());
        tabbedPane.addTab("Graphs", new SimpleGraphsPanel());
        tabbedPane.addTab("Statistics", new JPanel());

        return tabbedPane;
    }

}
