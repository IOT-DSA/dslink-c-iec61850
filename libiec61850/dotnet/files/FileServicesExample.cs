using System;
using System.Collections.Generic;

using IEC61850.Client;
using IEC61850.Common;
using System.IO;

namespace files
{
	class MainClass
	{
		public static void printFiles (IedConnection con, string prefix, string parent)
		{
			List<FileDirectoryEntry> files = con.GetFileDirectory (parent);

			foreach (FileDirectoryEntry file in files) {
				Console.WriteLine(prefix + file.GetFileName() + "\t" + file.GetFileSize() + "\t" + 
				                  MmsValue.MsTimeToDateTimeOffset(file.GetLastModified()));

				if (file.GetFileName().EndsWith("/")) {
					printFiles (con, prefix + "  ", parent + file.GetFileName());
				}
			}

		}

		static bool getFileHandler (object parameter, byte[] data)
		{
			Console.WriteLine("received " + data.Length + " bytes");

			BinaryWriter binWriter = (BinaryWriter) parameter;

			binWriter.Write(data);

			return true;
		}


		public static void Main (string[] args)
		{
			IedConnection con = new IedConnection ();

            string hostname;

            if (args.Length > 0)
                hostname = args[0];
            else
                hostname = "10.0.2.2";

            Console.WriteLine("Connect to " + hostname);

            try
            {
                con.Connect(hostname, 102);

				Console.WriteLine ("Files in server root directory:");
                List<string> serverDirectory = con.GetServerDirectory(true);

				foreach (string entry in serverDirectory) {
					Console.WriteLine(entry);
				}

				Console.WriteLine();

				Console.WriteLine ("File directory tree at server:");
				printFiles(con, "", "");
				Console.WriteLine();

				string filename = "IEDSERVER.BIN";

				Console.WriteLine("Download file " + filename);

				/* Download file from server and write it to a new local file */
				FileStream fs = new FileStream(filename, FileMode.Create);
            	BinaryWriter w = new BinaryWriter(fs);

				con.GetFile(filename, new IedConnection.GetFileHandler(getFileHandler), w);

				fs.Close();

                con.Abort();
            }
            catch (IedConnectionException e)
            {
				Console.WriteLine(e.Message);
            }

			// release all resources - do NOT use the object after this call!!
			con.Dispose ();
		}
	}
}
