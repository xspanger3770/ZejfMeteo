package exception;

public class FatalApplicationException extends Exception implements FatalError {

	public FatalApplicationException(Throwable cause) {
		super(cause);
	}
	
	public FatalApplicationException(String message, Throwable cause) {
		super(message, cause);
	}

	@Override
	public String getUserMessage() {
		return super.getMessage();
	}

}
