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
    public partial class tripForm : Form
    {
        public int tripNum = -1;

        public tripForm()
        {
            InitializeComponent();
        }

        private void cancelButton_Click(object sender, EventArgs e)
        {
            this.Close();
        }

        private void acceptButton_Click(object sender, EventArgs e)
        {
            try
            {
                tripNum = Convert.ToInt32(tripNumber.Text);
            }
            catch
            {
                MessageBox.Show("Invalid trip number!", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }

            this.Close();
        }
    }
}
