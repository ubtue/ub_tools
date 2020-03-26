package de.uni_tuebingen.ub.ixTheo.bibleRangeSearch;

import java.util.concurrent.TimeUnit;

public class SpeedTests {
	public static void main(String[] args) {
		for (int i = 0; i < 10; i++) {
			testSplitStrings();
			System.out.println("------------------");
		}
	}

	public static void testSplitStrings() {
		int REPEATS = 10000000;
		String s = randomString(15);
		System.out.println(s);
	
		int c = 0;
		TimeWatch watch = TimeWatch.start();
		for (int i = 0; i < REPEATS; i++) {
			c += s.split("_").length;
		}
		print("split", watch);
		watch.reset();

		for (int i = 0; i < REPEATS; i++) {
			c += s.substring(0, 7).length();
			c += s.substring(8, 15).length();
		}
		print("substring", watch);
		System.out.println(c);
	}

	private static String randomString(int length) {
		StringBuffer buffer = new StringBuffer();
		for (int i = 0; i < length; i++) {
			buffer.append((char) (Math.random() * 255));
		}
		buffer.setCharAt(7, '_');
		return buffer.toString();
	}

	private static void print(String name, TimeWatch watch) {
		System.out.println(name + " needed " + watch.time() + "ms");
	}

	public static class TimeWatch {
		long starts;

		public static TimeWatch start() {
			return new TimeWatch();
		}

		private TimeWatch() {
			reset();
		}

		public TimeWatch reset() {
			starts = System.currentTimeMillis();
			return this;
		}

		public long time() {
			long ends = System.currentTimeMillis();
			return ends - starts;
		}

		public long time(TimeUnit unit) {
			return unit.convert(time(), TimeUnit.MILLISECONDS);
		}
	}
}
