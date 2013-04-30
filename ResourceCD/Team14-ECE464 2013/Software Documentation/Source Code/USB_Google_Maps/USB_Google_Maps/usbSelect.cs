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

namespace USB_Google_Maps
{
    public partial class usbSelect : Form
    {
        public string usbName = "";
        public string path = "";

        public usbSelect()
        {
            InitializeComponent();
        }

        private void cancelButton_Click(object sender, EventArgs e)
        {
            this.Close();
        }

        private void acceptButton_Click(object sender, EventArgs e)
        {
            if (driveName.Text == "")
            {
                MessageBox.Show("No USB drive name entered!", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                return;
            }
            else
            {
                usbName = driveName.Text;
                DriveInfo[] drives = DriveInfo.GetDrives();

                foreach (DriveInfo d in drives)
                {
                    if (d.DriveType == DriveType.Removable && d.VolumeLabel == usbName)
                    {
                        path = d.Name;
                        break;
                    }                        
                }

                if (path == "")
                {
                    MessageBox.Show("Cannot find the drive you specified!", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    return;
                }
                else
                    MessageBox.Show("USB drive selected: " + usbName, "Success", MessageBoxButtons.OK, MessageBoxIcon.Information);
            }

            this.Close();
        }
    }
}
