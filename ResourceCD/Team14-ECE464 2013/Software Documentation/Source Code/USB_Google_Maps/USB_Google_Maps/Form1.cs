/*
 * 
 * Description: This Windows application will read in .txt files, located on a USB drive, that contain GPS locations, 
 *              and will parse them into a static map provided by Google Maps' web server.
 *              
 * Date: 4/25/2013
 * 
 * Author: Ben Paolillo 
 * 
 * Note: This application was made solely for Boston University's ECE Senior Design Team ReCycle. It should not be used
 *       for any other purpose.
 * 
 */

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;
using System.IO;
using System.Text.RegularExpressions;

namespace USB_Google_Maps
{
    public partial class Form1 : Form
    {
        private static usbSelect usb;
        private static tripForm trip;
        private static Form1 form = new Form1();

        private string path = "";
        private string line = "";
        private string url = "";
        private string urlEnd = "&sensor=false";

        private List<PointF> coords = new List<PointF>();
        private List<PointF> coordsDistinct = new List<PointF>();

        public Form1()
        {
            InitializeComponent();
            picture.Image = Properties.Resources.recycle_logo;
        }

        private void exitToolStripMenuItem_Click(object sender, EventArgs e)
        {
            Application.Exit();
        }

        private void selectUSBDriveToolStripMenuItem_Click(object sender, EventArgs e)
        {
            usb = new usbSelect();
            usb.ShowDialog();
        }

        private void plotTripRouteToolStripMenuItem_Click(object sender, EventArgs e)
        {
            try
            {
                if (usb != null && usb.path != "")
                {
                    trip = new tripForm();
                    trip.ShowDialog();
                }
                else
                {
                    MessageBox.Show("Please select a USB drive first!", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    return;
                }

                if (trip.tripNum == -1)
                    return;

                url = "http://maps.google.com/maps/api/staticmap?size=1024x1024&path=color:0x0000ff|weight:5|";
                path = usb.path + "ReCycle";
                coords.Clear();
                coordsDistinct.Clear();

                if (!Directory.Exists(path))
                {
                    MessageBox.Show("The directory /ReCycle was not found on your USB drive!\n" + "Now exiting.", "Error",
                        MessageBoxButtons.OK, MessageBoxIcon.Error);
                    return;
                }
                else
                {
                    path += "\\gps";

                    if (!Directory.Exists(path))
                    {
                        MessageBox.Show("The directory /ReCycle/gps was not found on your USB drive!\n" + "Now exiting.",
                            "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                        return;
                    }
                    else
                    {
                        path += "\\" + trip.tripNum.ToString() + ".txt";

                        if (!File.Exists(path))
                        {
                            picture.Image = Properties.Resources.recycle_logo;

                            MessageBox.Show("File not found: " + trip.tripNum.ToString() + ".txt",
                                "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);                    
                        }
                        else
                        {
                            StreamReader file = new StreamReader(path);

                            while ((line = file.ReadLine()) != null)
                            {
                                char[] delimiters = new char[] { '\t' };
                                string[] parts = line.Split(delimiters, StringSplitOptions.RemoveEmptyEntries);

                                string latStr = Regex.Replace(parts[2], "[^0-9.]", "");
                                string lngStr = Regex.Replace(parts[3], "[^0-9.]", "");

                                decimal lat = Convert.ToDecimal(latStr);
                                decimal lng = Convert.ToDecimal(lngStr);

                                PointF pt = new PointF((float)lat, (float)lng);
                                pt.Y *= -1;
                                coords.Add(pt);
                            }

                            coordsDistinct = coords.Distinct<PointF>().ToList<PointF>();

                            foreach (PointF p in coordsDistinct)
                            {
                                string point = p.X.ToString() + "," + p.Y.ToString();
                                url += point += "|";
                            }

                            url = url.TrimEnd('|');
                            url += urlEnd;

                            while (url.Length > 2000)
                            {
                                coords.Clear();
                                url = "http://maps.google.com/maps/api/staticmap?size=1024x1024&path=color:0x0000ff|weight:5|";

                                for (int i = 0; i < coordsDistinct.Count; i += 2)
                                {
                                    if (coordsDistinct.Count % 2 != 0 && i == coordsDistinct.Count - 1)
                                        break;

                                    float newLat = (coordsDistinct[i].X + coordsDistinct[i + 1].X) / 2;
                                    float newLng = (coordsDistinct[i].Y + coordsDistinct[i + 1].Y) / 2;

                                    PointF newPt = new PointF(newLat, newLng);
                                    coords.Add(newPt);
                                }

                                coordsDistinct = coords.Distinct<PointF>().ToList<PointF>();

                                foreach (PointF p in coordsDistinct)
                                {
                                    string point = p.X.ToString() + "," + p.Y.ToString();
                                    url += point += "|";
                                }

                                url = url.TrimEnd('|');
                                url += urlEnd;
                            }

                            picture.Load(url);

                            file.Close();
                        }
                    }
                }
            }
            catch(Exception ee)
            {
                picture.Image = Properties.Resources.recycle_logo;

                if (ee.Message == "The remote name could not be resolved: 'maps.google.com'")
                    MessageBox.Show("You are not connected to the internet!", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                else if (ee.Message == "Index was outside the bounds of the array.")
                    MessageBox.Show("Invalid text file!", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                else
                    MessageBox.Show("An unexpected error occurred. Sorry about that.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        private void clearMapToolStripMenuItem_Click(object sender, EventArgs e)
        {
            picture.Image = Properties.Resources.recycle_logo;
        }
    }
}
