package exception;

import action.CloseAction;
import action.IgnoreAction;
import action.TerminateAction;
import org.tinylog.Logger;

import javax.swing.*;
import java.awt.*;
import java.io.PrintWriter;
import java.io.StringWriter;

public class ApplicationErrorHandler implements Thread.UncaughtExceptionHandler {

	private final JFrame frame;

	public ApplicationErrorHandler(JFrame frame) {
		this.frame = frame;
	}

	@Override
	public void uncaughtException(Thread t, Throwable e) {
		Logger.error("An uncaught exception has occurred in thread {} : {}", t.getName(), e.getMessage());
		Logger.error(e);
		if(frame != null) {
			frame.dispose();
		}
		handleException(e);
		System.exit(1);
	}

	public synchronized void handleException(Throwable e) {
		System.err.println((e instanceof RuntimeApplicationException) + ", " + e.getCause());
		if (!(e instanceof RuntimeApplicationException)) {
			showDetailedError(e);
			return;
		}

		if (e instanceof FatalError ex) {
			showGeneralError(ex.getUserMessage(), true);
		} else {
			ApplicationException ex = (ApplicationException) e;
			showGeneralError(ex.getUserMessage(), false);
		}
	}

	int i = 0;

	private void showDetailedError(Throwable e) {
		i++;
		if (i == 2) {
			System.exit(0);
		}
		final Object[] options = getOptionsForDialog(true);
		JOptionPane.showOptionDialog(frame, createDetailedPane(e), "Fatal Error", JOptionPane.DEFAULT_OPTION,
				JOptionPane.ERROR_MESSAGE, null, options, null);
		i = 0;
	}

	private Component createDetailedPane(Throwable e) {
		JTextArea textArea = new JTextArea(16, 60);
		textArea.setEditable(false);
		StringWriter stackTraceWriter = new StringWriter();
		e.printStackTrace(new PrintWriter(stackTraceWriter));
		textArea.append(stackTraceWriter.toString());
		return new JScrollPane(textArea);
	}

	private void showGeneralError(String message, boolean isFatal) {
		final String title = isFatal ? "Fatal Error" : "Application Error";
		final Object[] options = getOptionsForDialog(isFatal);

		JOptionPane.showOptionDialog(frame, message, title, JOptionPane.DEFAULT_OPTION, JOptionPane.ERROR_MESSAGE, null,
				options, null);
	}

	private Component[] getOptionsForDialog(boolean isFatal) {
		if (!isFatal) {
			return null; // use default
		}

		return new Component[] { new JButton(new TerminateAction()), new JButton(new CloseAction(frame)),
				new JButton(new IgnoreAction()) };
	}
}
