import java.awt.Color;
import java.awt.Container;
import java.awt.Dimension;
import java.awt.Graphics;
import java.awt.Graphics2D;
import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;
import java.awt.event.MouseAdapter;
import java.awt.event.MouseEvent;
import java.awt.event.WindowAdapter;
import java.io.IOException;
import java.net.DatagramPacket;
import java.net.InetAddress;
import java.net.MulticastSocket;
import java.util.Arrays;

import javax.swing.Box;
import javax.swing.BoxLayout;
import javax.swing.JFrame;
import javax.swing.JLabel;
import javax.swing.JPanel;
import javax.swing.JTextField;

/**
 * Protocol with all data being little endian:
 * 
 * 4 byte header magic header
 * 
 * 0x59, 0x54, 0x4b, 0x4a 'YTKJ'
 * 
 * uint16_t controller id (1, 2, 3, 4, ...)
 * 
 * 0x01, 0x00
 * 
 * 2 byte unknown
 * 
 * 0x57, 0x05,
 * 
 * uint16_t number of ports in use. Since we only have 8 ports the maximum
 * number here is 8. For more ports you need to increment the controller id.
 * Remember each port could potentially cover 4 daisy-chained strips.
 * 
 * 0x01, 0x00
 * 
 * Next is 4 repeating bytes. 1x4 bytes for each port in use.
 * 
 * 2 byte channel (in our setup each port has 2048 channels). So to address port
 * 2 you would set 0x00, 0x02
 * 
 * 0x00, 0x00,
 * 
 * uint16_t number of LEDs connected to each port (remember to multiply by 3 for
 * RGB)
 * 
 * 0x80, 0x01
 */

@SuppressWarnings("serial")
class LEDDisplay extends JPanel {
	private static final int PIXEL_SIZE = 6;
	private int x;
	private int y;
	private byte[] ledScreen;

	public LEDDisplay(byte[] ledScreen, int x, int y) {
		this.ledScreen = ledScreen;
		this.x = x;
		this.y = y;
		ledScreen = new byte[x * y];
		addMouseListener(new MyMouseAdapter());
	}

	public void paintComponent(Graphics g) {
		clear(g);
		Graphics2D g2d = (Graphics2D) g;
		int i = 0;
		for (int iy = 0; iy < y; iy++) {
			for (int ix = 0; ix < x; ix++) {
				g2d.setPaint(Color.WHITE);
				g2d.drawRect(ix * (PIXEL_SIZE + 2), iy * (PIXEL_SIZE + 2),
						PIXEL_SIZE + 2, PIXEL_SIZE + 2);
				switch (ledScreen[i] % 4) {
				case 0:
					g2d.setPaint(Color.BLACK);
					break;
				case 1:
					g2d.setPaint(Color.RED);
					break;
				case 2:
					g2d.setPaint(Color.GREEN);
					break;
				default:
					g2d.setPaint(Color.BLUE);
					break;
				}
				g2d.fillRect(ix * (PIXEL_SIZE + 2) + 1, iy * (PIXEL_SIZE + 2)
						+ 1, PIXEL_SIZE, PIXEL_SIZE);
				i++;
			}
		}
	}

	protected void clear(Graphics g) {
		super.paintComponent(g);
	}

	public void setRes(byte[] ledScreen, int x, int y) {
		this.x = x;
		this.y = y;
		this.ledScreen = ledScreen;
		repaint();
	}

	private final class MyMouseAdapter extends MouseAdapter {
		@Override
		public void mousePressed(MouseEvent e) {
			super.mousePressed(e);

			try {
				ledScreen[e.getX() / (PIXEL_SIZE + 2) + x
						* (e.getY() / (PIXEL_SIZE + 2))]++;
				repaint();
			} catch (ArrayIndexOutOfBoundsException f) {
			}
		}
	}
}

public class SimpleTouch {
	private static final int PORTS_PR_CONTROLLER = 8;
	private static final int PORT_MAP[] = new int[] { 58, 56, 48, 48 };
	private static final int NUMBER_OF_CONTROLLERS = 1;
	private static final int NUMBER_OF_PORTS_IN_USE = 2;
	private static int xRes = 4;
	private static int yRes = 58;
	private final MulticastSocket socket;
	private final InetAddress dest;
	private byte[] ledScreen;
	private final LEDDisplay myLEDDisplay;

	public static JFrame openInJFrame(Container content, int width, int height,
			String title, Color bgColor) {
		JFrame frame = new JFrame(title);
		frame.setBackground(bgColor);
		content.setBackground(bgColor);
		frame.setSize(width, height);
		frame.setContentPane(content);
		frame.addWindowListener(new WindowAdapter() {
			public void windowClosing(java.awt.event.WindowEvent e) {
				System.exit(0);
			};
		});
		frame.setVisible(true);
		return (frame);
	}

	private SimpleTouch() throws IOException {
		socket = new MulticastSocket();
		dest = InetAddress.getByName("224.1.1.1");
		ledScreen = new byte[xRes * yRes];

		myLEDDisplay = new LEDDisplay(ledScreen, xRes, yRes);
		JPanel config = new JPanel();
		config.setMaximumSize(new Dimension(100, Integer.MAX_VALUE));
		config.setLayout(new BoxLayout(config, BoxLayout.Y_AXIS));
		config.add(new JLabel("X resolution"));
		final JTextField xResTextfield = new JTextField(Integer.toString(xRes));
		xResTextfield.addActionListener(new ActionListener() {
			@Override
			public void actionPerformed(ActionEvent e) {
				xRes = Integer.valueOf(xResTextfield.getText());
				setRes(xRes, yRes);
			}
		});
		xResTextfield.setMaximumSize(new Dimension(50, 30));
		config.add(xResTextfield);
		config.add(new JLabel("Y resolution"));
		final JTextField yResTextfield = new JTextField(Integer.toString(yRes));
		yResTextfield.addActionListener(new ActionListener() {
			@Override
			public void actionPerformed(ActionEvent e) {
				yRes = Integer.valueOf(yResTextfield.getText());
				setRes(xRes, yRes);
			}
		});
		yResTextfield.setMaximumSize(new Dimension(50, 30));
		config.add(yResTextfield);
		config.add(Box.createHorizontalGlue());
		JPanel main = new JPanel();
		main.setLayout(new BoxLayout(main, BoxLayout.X_AXIS));
		main.add(myLEDDisplay);
		main.add(config);
		openInJFrame(main, 640, 500, "SimpleTouch", Color.WHITE);

		new Thread("Sine") {
			@Override
			public void run() {
				int sine_offset = 0;
				while (true) {
					// Simple stuff that draws a moving sine
					Arrays.fill(ledScreen, (byte) 0);
					for (int y = 0; y < yRes; y++) {
						int offset = (int) (Math.sin(sine_offset / 2 + 2
								* Math.PI * y / yRes)
								* xRes + 0.5);
						offset = (offset + xRes) / 2;
						if (offset < 0)
							offset = 0;
						else if (offset >= xRes)
							offset = xRes - 1;
						ledScreen[y * xRes + offset] = (byte) y;
					}
					try {
						myLEDDisplay.repaint();
						Thread.sleep(100);
						update(ledScreen);
					} catch (InterruptedException e) {
					}
					sine_offset++;
				}
			};
		}.start();
	}

	private void setRes(int xRes2, int yRes2) {
		ledScreen = new byte[xRes * yRes];
		myLEDDisplay.setRes(ledScreen, xRes, yRes);
	}

	public static void main(String args[]) throws IOException {
		new SimpleTouch();
	}

	private void update(byte[] ledScreen) {
		byte[] payload = new byte[2702];

		payload[0] = 'Y';
		payload[1] = 'T';
		payload[2] = 'K';
		payload[3] = 'J';

		int ix = 0;

		for (int controller = 1; controller <= NUMBER_OF_CONTROLLERS; controller++) {
			payload[4] = (byte) (controller);
			payload[5] = 0;

			// Unknown
			payload[6] = 0x57;
			payload[7] = 0x05;
			int portsInUse = NUMBER_OF_PORTS_IN_USE - (controller - 1)
					* PORTS_PR_CONTROLLER;
			if (portsInUse > PORTS_PR_CONTROLLER) {
				portsInUse = PORTS_PR_CONTROLLER;
			}

			payload[8] = (byte) portsInUse;
			payload[9] = 0;

			int payloadIndex = 10;

			// Now map the pixels
			for (int port = 0, channel = 0; port < portsInUse; port++, channel += 2048) {
				payload[payloadIndex++] = (byte) (channel & 0xff);
				payload[payloadIndex++] = (byte) ((channel >> 8) & 0xff);

				int ledsOnPort = PORT_MAP[port * 2] + PORT_MAP[port * 2 + 1];
				payload[payloadIndex++] = (byte) ((ledsOnPort * 3) & 0xff);
				payload[payloadIndex++] = (byte) (((ledsOnPort * 3) >> 8) & 0xff);

				for (int iy = PORT_MAP[port * 2] - 1; iy >= 0; iy -= 2) {
					setPixelPayload(ix, iy, ledScreen, payload, payloadIndex);
					payloadIndex += 3;
				}
				for (int iy = 0; iy < PORT_MAP[port * 2 + 1]; iy += 2) {
					setPixelPayload(ix, iy, ledScreen, payload, payloadIndex);
					payloadIndex += 3;
				}
				ix++;
				for (int iy = PORT_MAP[port * 2] - 1; iy >= 0; iy -= 2) {
					setPixelPayload(ix, iy, ledScreen, payload, payloadIndex);
					payloadIndex += 3;
				}
				for (int iy = 0; iy < PORT_MAP[port * 2 + 1]; iy += 2) {
					setPixelPayload(ix, iy, ledScreen, payload, payloadIndex);
					payloadIndex += 3;
				}
				ix++;
			}

			// Payload is done. Send it
			DatagramPacket dp = new DatagramPacket(payload, payload.length);
			dp.setPort(1097);
			dp.setAddress(dest);

			try {
				socket.send(dp);
			} catch (IOException e) {
			}
		}
	}

	private void setPixelPayload(int ix, int iy, byte[] ledScreen,
			byte[] payload, int payloadIndex) {
		int rgb;
		switch (ledScreen[ix + iy * xRes] % 4) {
		case 0:
			rgb = 0x0;
			break;
		case 1:
			rgb = 0x00007f;
			break;
		case 2:
			rgb = 0x007f00;
			break;
		default:
			rgb = 0x7f0000;
			break;
		}
		payload[payloadIndex] = (byte) (rgb & 0xff);
		payload[payloadIndex + 1] = (byte) ((rgb >> 8) & 0xff);
		payload[payloadIndex + 2] = (byte) ((rgb >> 16) & 0xff);
	}
}