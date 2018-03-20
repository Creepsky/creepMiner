using System.ComponentModel;

namespace ConfigEditor.Models
{
	public class Base
    {
        private string name;


        public string Name
        {
            get { return name; }
            set
            {
                if (name != value)
                {
                    name = value;
                }
            }
        }

        public Base(string name)
        {
            this.Name = name;
        }
    }
}